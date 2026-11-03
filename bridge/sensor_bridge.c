// 本地传感器桥接层（只跑在 i.MX6ULL 上，不涉及 CAN / 外部节点）
//
// 当前上板的两路（都走你已有的内核驱动）：
//   15-dht11_platform  →  /dev/dht11_misc      platform + misc 设备   驾驶舱温湿度
//   20-mpu6050         →  /dev/mpu6050_misc    I2C + misc 设备        车身姿态
//
// 发动机温度、倒车距离这两块面板保留在界面上，但本板没有数据源，
// 固定显示"未接入"（灰色离线）。
// 以后想补数据，重写文件末尾的两个弱函数 bridge_ext_engine_temp() /
// bridge_ext_rear_distance() 就行，采集线程会自动把它喂给界面，UI 侧不用改。
//
// 这两路的数据格式完全照驱动源码里 copy_to_user 的布局解析，不要凭猜。

#define _GNU_SOURCE

#include "sensor_bridge.h"
#include "ui_sensor.h"
#include "gw_msg.h"         /* 单元名字 + msgdata.id 约定 + 群发 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <sched.h>
#include <pthread.h>

/* ==========================================================================
 *  1. 配置区
 * ========================================================================== */

#define DEV_DHT11           "/dev/dht11_misc"
#define DEV_MPU6050         "/dev/mpu6050_misc"

/* MPU6050 量程 —— 必须和你驱动里写的一致！
 * 你的 mpu6050_drv.c 里：
 *   MPU6050_ACCEL_CONFIG = 0x08  → ±4g   → 8192 LSB/g
 *   MPU6050_GYRO_CONFIG  = 0x08  → ±500°/s → 65.5 LSB/(°/s)
 * 如果哪天把 ACCEL_CONFIG 改成 0x00（±2g），这里要跟着改成 16384
 */
#define MPU_ACCEL_LSB_PER_G     8192.0f
#define MPU_GYRO_LSB_PER_DPS    65.5f

/* 互补滤波系数：越大越相信陀螺（短期平滑），越小越相信加速度计（长期不漂）。
 * 车身振动大的卡车建议 0.96~0.98。 */
#define MPU_COMP_ALPHA          0.98f

/* 安装方向修正：传感器贴反了（pitch/roll 正负颠倒）就改成 -1.0f */
#define PITCH_SIGN              1.0f
#define ROLL_SIGN               1.0f

/* 开机校准（每次启动、或驱动重连后自动做一次，约 3 秒）：
 *  1) 测陀螺零偏 —— 静态时 gx/gy 读到的不一定是 0（你的模块 gx≈-255），
 *     不减掉会一直往一个方向积分，画面上就是"数字自己乱跑"；
 *  2) 把开机时的姿态记成 0° —— 模块横装、竖装、侧装都能正常显示。
 *
 * 要求：启动那 3 秒车（或桌子）别动。
 * 如果你的车平时就停在坡上，不想要"开机即 0°"，把 ATT_AUTO_ZERO 改 0，
 * 那样输出的是芯片坐标系的绝对角（水平面为 0°，模块斜装时会一直有偏置）。 */
#define ATT_AUTO_ZERO           1
#define ATT_CAL_SAMPLES         60          /* 50ms 一拍 × 60 ≈ 3 秒 */
#define ATT_STILL_TOL_G         0.15f       /* 合加速度离 1g 超过这个值就不采信 */
#define ATT_VIB_TOL_G           0.30f       /* 运行中超过这个值，这一拍不做加速度计纠正 */
#define ATT_DEBUG_LOG           1           /* 1 = 每秒往串口打一行原始值和角度，调试完可关 */

/* 采样周期 */
#define PERIOD_MS_MPU           50
#define PERIOD_MS_DHT11         2000        /* 驱动里是 msleep(20)+几十 ms，2s 一次稳 */
#define PERIOD_MS_CLOUD         5000
#define PERIOD_MS_REAR          200         /* 倒车距离是预留通道，没接时只是空转 */

/* 连续失败几次才认为"离线"。单次失败通常只是时序抖动，不该让面板闪 */
#define FAIL_NUM_TO_OFFLINE     3

/* 是否在串口打印状态变化（上线/掉线），调试期开着方便 */
#define BRIDGE_VERBOSE          1

/* 采集线程是否用 SCHED_FIFO 提优先级（数字是优先级，0 = 不提权）。
 * 提权能减少姿态/测距的节拍抖动，但需要 root；
 * 非 root 运行时会自动退回普通调度，不会导致启动失败。 */
#define SENSOR_THREAD_PRIO      30

/* ==========================================================================
 *  2. 工具
 * ========================================================================== */

static volatile bool g_run = false;
static pthread_t     g_tid_fast;
static pthread_t     g_tid_slow;
static bool          g_fast_started = false;
static bool          g_slow_started = false;

/* 每路的连续失败计数：只有连续失败够多次才把面板打成离线，
 * 避免偶发一次读失败就让数字在"数值 / --"之间闪 */
static int           g_att_fail  = 0;
static int           g_dht_fail  = 0;
static int           g_rear_fail = 0;
static int           g_eng_fail  = 0;

static uint32_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u);
}

static uint64_t now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000ull;
}

/* 读满 len 字节，返回 0 成功；失败返回 -1（errno 保留原因） */
static int read_exact(int fd, void * buf, size_t len)
{
    ssize_t n = read(fd, buf, len);
    if(n == (ssize_t)len) return 0;
    return -1;
}

static void log_state(const char * tag, bool online, int err)
{
#if BRIDGE_VERBOSE
    static bool last[2] = { true, true };
    int idx;

    /* 只跟踪本板两路；其余两路（机温/倒车）走预留通道，状态由那边自己刷 */
    if(strcmp(tag, "DHT11") == 0)         idx = 0;
    else if(strcmp(tag, "MPU6050") == 0)  idx = 1;
    else return;

    if(last[idx] == online) return;
    last[idx] = online;

    if(online) fprintf(stderr, "[bridge] %s 已上线\n", tag);
    else       fprintf(stderr, "[bridge] %s 离线（errno=%d %s）\n", tag, err, strerror(err));
#else
    (void)tag; (void)online; (void)err;
#endif
}

/* ==========================================================================
 *  3. DHT11 —— /dev/dht11_misc
 *
 *  驱动源码（dht11_platform_drv.c）里的约定：
 *      data[0] = 湿度整数   data[1] = 湿度小数
 *      data[2] = 温度整数   data[3] = 温度小数
 *  成功返回 4，失败返回负数（-1 无响应 / -2 校验错 / -3 拷贝失败）。
 *  到用户态表现为 read() == -1，errno 是 1/2/3。
 * ========================================================================== */

static int  g_fd_dht11 = -1;
static int  g_dht11_fail = 0;
static uint32_t g_dht11_next_try = 0;

static bool dht11_ensure_open(void)
{
    uint32_t t = now_ms();

    if(g_fd_dht11 >= 0) return true;
    if((int32_t)(t - g_dht11_next_try) < 0) return false;   /* 还没到重试时间 */
    g_dht11_next_try = t + 5000;                            /* 5 秒重试一次 */

    g_fd_dht11 = open(DEV_DHT11, O_RDWR);
    if(g_fd_dht11 < 0) {
        log_state("DHT11", false, errno);
        return false;
    }
    return true;
}

static bool dht11_poll(float * temp_c, float * humi_rh)
{
    unsigned char d[4] = { 0 };

    if(!dht11_ensure_open()) return false;

    if(read_exact(g_fd_dht11, d, sizeof(d)) < 0) {
        int err = errno;

        /* 驱动返回 -1/-2 时内核会关掉 fd 吗？不会，但连续失败要重开 */
        if(++g_dht11_fail >= FAIL_NUM_TO_OFFLINE) {
            close(g_fd_dht11);
            g_fd_dht11 = -1;
            g_dht11_next_try = now_ms() + 5000;
            log_state("DHT11", false, err);
        }
        return false;
    }

    g_dht11_fail = 0;
    log_state("DHT11", true, 0);

    *humi_rh = (float)d[0] + (float)d[1] * 0.1f;
    *temp_c  = (float)d[2] + (float)d[3] * 0.1f;

    /* 合法性检查：DHT11 量程 0~50℃ / 20~90%RH，越界说明数据不可信 */
    if(*temp_c < -10.0f || *temp_c > 60.0f || *humi_rh < 5.0f || *humi_rh > 99.0f) {
        return false;
    }
    return true;
}

/* ==========================================================================
 *  4. MPU6050 —— /dev/mpu6050_misc
 *
 *  驱动返回 14 字节原始数据，顺序（大端 int16）：
 *      [0..1] ax  [2..3] ay  [4..5] az  [6..7] 温度  [8..9] gx  [10..11] gy  [12..13] gz
 * ========================================================================== */

static int      g_fd_mpu = -1;
static int      g_mpu_fail = 0;
static uint32_t g_mpu_next_try = 0;
static float    g_pitch = 0.0f;         /* 滤波后的姿态角（芯片坐标系） */
static float    g_roll  = 0.0f;
static bool     g_att_inited = false;
static uint64_t g_mpu_last_us = 0;

/* ---- 开机校准的状态 ---- */
static bool     g_cal_done = false;
static int      g_cal_n = 0;
static uint32_t g_cal_deadline = 0;
static double   g_cal_gx_sum = 0.0, g_cal_gy_sum = 0.0;
static double   g_cal_pitch_sum = 0.0, g_cal_roll_sum = 0.0;
static float    g_bias_gx_dps = 0.0f, g_bias_gy_dps = 0.0f;
static float    g_zero_pitch = 0.0f, g_zero_roll = 0.0f;
static uint32_t g_dbg_next = 0;

/* 结束校准：用攒到的样本算零偏和零点；一个样本都没有就退化成"不补偿" */
static void mpu_cal_finish(void)
{
    if(g_cal_n > 0) {
        g_bias_gx_dps = (float)(g_cal_gx_sum / g_cal_n);
        g_bias_gy_dps = (float)(g_cal_gy_sum / g_cal_n);
        g_zero_pitch  = ATT_AUTO_ZERO ? (float)(g_cal_pitch_sum / g_cal_n) : 0.0f;
        g_zero_roll   = ATT_AUTO_ZERO ? (float)(g_cal_roll_sum  / g_cal_n) : 0.0f;
    }
    g_cal_done   = true;
    g_att_inited = false;

    fprintf(stderr,
            "[mpu] 校准完成：陀螺零偏 gx=%+.2f gy=%+.2f °/s，"
            "零点 P=%+.1f R=%+.1f °（%d 个样本）\n",
            (double)g_bias_gx_dps, (double)g_bias_gy_dps,
            (double)g_zero_pitch, (double)g_zero_roll, g_cal_n);
}

static bool mpu_ensure_open(void)
{
    uint32_t t = now_ms();

    if(g_fd_mpu >= 0) return true;
    if((int32_t)(t - g_mpu_next_try) < 0) return false;
    g_mpu_next_try = t + 5000;

    g_fd_mpu = open(DEV_MPU6050, O_RDWR);
    if(g_fd_mpu < 0) {
        log_state("MPU6050", false, errno);
        return false;
    }

    /* 重连后重新定初值、重新校准（板子可能被挪过位置） */
    g_att_inited   = false;
    g_cal_done     = false;
    g_cal_n        = 0;
    g_cal_deadline = t + 20000;         /* 最多等 20s，车一直动也不会卡在校准里 */
    return true;
}

static bool mpu_poll(float * pitch_deg, float * roll_deg)
{
    unsigned char d[14];
    int16_t ax, ay, az, gx, gy;
    float   ax_g, ay_g, az_g, amag;
    float   gx_raw_dps, gy_raw_dps, gx_dps, gy_dps, dt, acc_w;
    float   pitch_acc, roll_acc;
    uint64_t t_us;

    if(!mpu_ensure_open()) return false;

    if(read_exact(g_fd_mpu, d, sizeof(d)) < 0) {
        int err = errno;

        if(++g_mpu_fail >= FAIL_NUM_TO_OFFLINE) {
            close(g_fd_mpu);
            g_fd_mpu = -1;
            g_mpu_next_try = now_ms() + 5000;
            log_state("MPU6050", false, err);
        }
        return false;
    }

    g_mpu_fail = 0;
    log_state("MPU6050", true, 0);

    ax = (int16_t)(((uint16_t)d[0] << 8) | d[1]);
    ay = (int16_t)(((uint16_t)d[2] << 8) | d[3]);
    az = (int16_t)(((uint16_t)d[4] << 8) | d[5]);
    gx = (int16_t)(((uint16_t)d[8] << 8) | d[9]);
    gy = (int16_t)(((uint16_t)d[10] << 8) | d[11]);

    ax_g = (float)ax / MPU_ACCEL_LSB_PER_G;
    ay_g = (float)ay / MPU_ACCEL_LSB_PER_G;
    az_g = (float)az / MPU_ACCEL_LSB_PER_G;
    gx_raw_dps = (float)gx / MPU_GYRO_LSB_PER_DPS;
    gy_raw_dps = (float)gy / MPU_GYRO_LSB_PER_DPS;

    /* 合加速度：静止时应该等于 1g，偏离太多说明车在动 / 震动大 */
    amag = sqrtf(ax_g * ax_g + ay_g * ay_g + az_g * az_g);

    /* 加速度计直接算出的角度（长期准，但振动噪声大） */
    pitch_acc = atan2f(-ax_g, sqrtf(ay_g * ay_g + az_g * az_g)) * 57.29578f;
    roll_acc  = atan2f(ay_g, az_g) * 57.29578f;

    /* ---------- ① 开机校准 ----------
     * 做两件事：
     *   a) 测出陀螺零偏（你的模块 gx 静态就有 -255 ≈ -3.9°/s，
     *      不减掉的话角度会一直往一个方向飘，看起来就像"随机"）
     *   b) 把开机时的姿态记成 0°（模块横装/竖装都能正常显示）
     * 只采信合加速度接近 1g 的样本，车在动就不算。 */
    if(!g_cal_done) {
        if(fabsf(amag - 1.0f) < ATT_STILL_TOL_G) {
            g_cal_gx_sum    += gx_raw_dps;
            g_cal_gy_sum    += gy_raw_dps;
            g_cal_pitch_sum += pitch_acc;
            g_cal_roll_sum  += roll_acc;
            g_cal_n++;
        }

        if(g_cal_n >= ATT_CAL_SAMPLES || (int32_t)(now_ms() - g_cal_deadline) >= 0) {
            mpu_cal_finish();           /* 样本够了、或者超时了，就这么定下来 */
        }
        else {
            /* 校准期间先输出瞬时角度，界面不至于一直显示 -- */
            *pitch_deg = (pitch_acc - g_zero_pitch) * PITCH_SIGN;
            *roll_deg  = (roll_acc  - g_zero_roll)  * ROLL_SIGN;
            return true;
        }
    }

    /* 减掉零偏后的角速度 */
    gx_dps = gx_raw_dps - g_bias_gx_dps;
    gy_dps = gy_raw_dps - g_bias_gy_dps;

    t_us = now_us();
    if(!g_att_inited) {
        g_pitch = pitch_acc;
        g_roll  = roll_acc;
        g_att_inited = true;
        g_mpu_last_us = t_us;
    }
    else {
        dt = (float)(t_us - g_mpu_last_us) / 1000000.0f;
        g_mpu_last_us = t_us;

        /* dt 异常说明线程被调度卡住了，这一拍就别积分了，否则姿态会飞 */
        if(dt <= 0.0f || dt > 0.5f) dt = PERIOD_MS_MPU / 1000.0f;

        /* 车在颠的时候（合加速度明显偏离 1g）只靠陀螺积分，
         * 不做加速度计纠正，否则振动噪声会把角度拽得乱跳 */
        acc_w = (fabsf(amag - 1.0f) < ATT_VIB_TOL_G) ? (1.0f - MPU_COMP_ALPHA) : 0.0f;

        g_pitch = (1.0f - acc_w) * (g_pitch + gy_dps * dt) + acc_w * pitch_acc;
        g_roll  = (1.0f - acc_w) * (g_roll  + gx_dps * dt) + acc_w * roll_acc;
    }

    /* 输出的是"相对开机姿态"的角度，横装竖装都从 0° 起算 */
    *pitch_deg = (g_pitch - g_zero_pitch) * PITCH_SIGN;
    *roll_deg  = (g_roll  - g_zero_roll)  * ROLL_SIGN;

#if ATT_DEBUG_LOG
    {
        uint32_t tms = now_ms();
        if((int32_t)(tms - g_dbg_next) >= 0) {
            g_dbg_next = tms + 1000;
            fprintf(stderr,
                    "[mpu] raw ax=%6d ay=%7d az=%6d gx=%6d gy=%6d | |a|=%.2fg | "
                    "acc P=%+6.1f R=%+6.1f | out P=%+6.1f R=%+6.1f%s\n",
                    ax, ay, az, gx, gy, (double)amag,
                    (double)pitch_acc, (double)roll_acc,
                    (double)*pitch_deg, (double)*roll_deg,
                    g_cal_done ? "" : "  <校准中>");
        }
    }
#endif

    /* 只有明显不合理（>150°）才判失败，正常大角度倾斜不该被丢掉 */
    if(fabsf(*pitch_deg) > 150.0f || fabsf(*roll_deg) > 150.0f) return false;
    return true;
}

/* ==========================================================================
 *  5. 云平台状态 —— 留钩子，接 MQTT/HTTP 时实现它
 * ========================================================================== */

__attribute__((weak))
bool bridge_cloud_status(uint32_t * tx_count, uint32_t * last_tx_s,
                         int8_t * rssi_dbm)
{
    *tx_count  = 0;
    *last_tx_s = 0;
    *rssi_dbm  = -127;
    return false;                       /* 恒离线 */
}

/* ==========================================================================
 *  6. 预留通道 —— 发动机温度 / 倒车距离
 *
 *  板上没有这两个传感器，默认实现返回 false，界面显示"未接入"。
 *  以后有了数据源（直连 i.MX6ULL 的 DS18B20、超声波，或者任何总线），
 *  在你自己的 .c 里重写下面两个同名函数即可，签名保持一模一样：
 *
 *      bool bridge_ext_engine_temp(float * temp_c)
 *      {
 *          *temp_c = 92.5f;
 *          return true;            // 返回 false 表示这一拍没有有效数据
 *      }
 *
 *  这样就不必改采集线程，也不必动 UI。
 * ========================================================================== */

__attribute__((weak))
bool bridge_ext_engine_temp(float * temp_c)
{
    (void)temp_c;
    return false;
}

__attribute__((weak))
bool bridge_ext_rear_distance(float * dist_m, bool * reverse)
{
    (void)dist_m;
    (void)reverse;
    return false;
}

/* ==========================================================================
 *  6.5 往邮箱投递（采集单元 → 显示 / 预警 / 上传）
 *
 *  采集线程只负责把数据打包扔进邮箱，不直接碰界面，也不做阈值判断。
 *  三个消费单元各收一份，谁慢谁快互不影响。
 * ========================================================================== */

static void _post_cabin(float t, float h, sensor_status_t st)
{
    struct msgdata d;

    memset(&d, 0, sizeof(d));
    d.id     = GW_ID_CABIN;
    d.temp   = t;
    d.hum    = h;
    d.status = (int)st;
    gw_post_acq(&d);
}

static void _post_engine(float t, sensor_status_t st)
{
    struct msgdata d;

    memset(&d, 0, sizeof(d));
    d.id     = GW_ID_ENGINE;
    d.temp   = t;
    d.status = (int)st;
    gw_post_acq(&d);
}

static void _post_att(float pitch, float roll, sensor_status_t st)
{
    struct msgdata d;

    memset(&d, 0, sizeof(d));
    d.id     = GW_ID_ATTITUDE;
    d.pitch  = pitch;
    d.roll   = roll;
    d.status = (int)st;
    gw_post_acq(&d);
}

static void _post_rear(float dist, bool reverse, sensor_status_t st)
{
    struct msgdata d;

    memset(&d, 0, sizeof(d));
    d.id      = GW_ID_REAR;
    d.dist    = dist;
    d.reverse = reverse ? 1 : 0;
    d.status  = (int)st;
    gw_post_acq(&d);
}

/* ==========================================================================
 *  7. 采集线程
 * ========================================================================== */

static void * _thread_fast(void * arg)
{
    uint32_t next_mpu, next_rear;
    uint32_t t0 = now_ms();

    (void)arg;

    next_mpu  = t0;
    next_rear = t0;

    while(g_run) {
        uint32_t t = now_ms();
        float    pitch = 0.0f, roll = 0.0f;

        if((int32_t)(t - next_mpu) >= 0) {
            next_mpu = t + PERIOD_MS_MPU;
            if(mpu_poll(&pitch, &roll)) {
                g_att_fail = 0;
                _post_att(pitch, roll, SENSOR_OK);
            }
            /* 偶发一次读失败不要把面板打成 "--"（会一闪一闪看着像乱码），
             * 连续 FAIL_NUM_TO_OFFLINE 次才判离线，而且只在跳变时推一次 */
            else if(++g_att_fail == FAIL_NUM_TO_OFFLINE) {
                _post_att(0.0f, 0.0f, SENSOR_TIMEOUT);
            }
        }

        /* 倒车距离：预留通道。没人实现弱函数时只推一次离线，
         * 面板就是灰的"未接入"，不会有假数据 */
        if((int32_t)(t - next_rear) >= 0) {
            float dm  = 0.0f;
            bool  rev = false;

            next_rear = t + PERIOD_MS_REAR;
            if(bridge_ext_rear_distance(&dm, &rev)) {
                g_rear_fail = 0;
                _post_rear(dm, rev, SENSOR_OK);
            }
            else if(++g_rear_fail == FAIL_NUM_TO_OFFLINE) {
                _post_rear(0.0f, false, SENSOR_TIMEOUT);
            }
        }

        /* 云端在线状态改由"网络通信单元"维护（它知道自己有没有发成功），
         * 采集单元不再插手，避免两边互相覆盖。 */

        usleep(5000);
    }
    return NULL;
}

static void * _thread_slow(void * arg)
{
    uint32_t next_dht, next_eng;
    uint32_t t0 = now_ms();

    (void)arg;

    next_dht = t0;
    next_eng = t0;

    while(g_run) {
        uint32_t t = now_ms();

        if((int32_t)(t - next_dht) >= 0) {
            float tc = 0.0f, rh = 0.0f;

            next_dht = t + PERIOD_MS_DHT11;
            /* DHT11 → 驾驶舱温湿度。同样是连续失败才判离线，
             * 单次时序抖动不应该让面板闪 "--" */
            if(dht11_poll(&tc, &rh)) {
                g_dht_fail = 0;
                _post_cabin(tc, rh, SENSOR_OK);
            }
            else if(++g_dht_fail == FAIL_NUM_TO_OFFLINE) {
                _post_cabin(0.0f, 0.0f, SENSOR_TIMEOUT);
            }
        }

        /* 发动机温度：同样是预留通道，默认恒离线 */
        if((int32_t)(t - next_eng) >= 0) {
            float te = 0.0f;

            next_eng = t + 1000;
            if(bridge_ext_engine_temp(&te)) {
                g_eng_fail = 0;
                _post_engine(te, SENSOR_OK);
            }
            else if(++g_eng_fail == FAIL_NUM_TO_OFFLINE) {
                _post_engine(0.0f, SENSOR_TIMEOUT);
            }
        }

        usleep(20000);
    }
    return NULL;
}

/* ==========================================================================
 *  8. 对外接口
 * ========================================================================== */

/* 起线程：优先用实时调度，失败（非 root / EPERM）就退回普通调度。
 * 不能因为提权失败就让整个采集起不来。 */
static int _spawn(pthread_t * tid, void * (*fn)(void *), const char * name)
{
    pthread_attr_t     attr;
    struct sched_param sp;
    int                ret;

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 256 * 1024);

#if SENSOR_THREAD_PRIO > 0
    if(geteuid() == 0) {
        pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
        sp.sched_priority = SENSOR_THREAD_PRIO;
        pthread_attr_setschedparam(&attr, &sp);
    }
#endif

    ret = pthread_create(tid, &attr, fn, NULL);
    pthread_attr_destroy(&attr);

    if(ret != 0 && ret == EPERM) {
        /* 提权被拒，退回默认调度再试一次 */
        ret = pthread_create(tid, NULL, fn, NULL);
        if(ret == 0) {
            fprintf(stderr, "[bridge] %s 未获得实时优先级，降级为普通调度\n", name);
        }
    }

    if(ret != 0) {
        fprintf(stderr, "[bridge] %s 创建失败: %s\n", name, strerror(ret));
        return -1;
    }
    return 0;
}

int sensor_bridge_start(void)
{
    if(g_run) return 0;
    g_run = true;

    if(_spawn(&g_tid_fast, _thread_fast, "快线程(姿态)") < 0) {
        g_run = false;
        return -1;
    }
    g_fast_started = true;

    if(_spawn(&g_tid_slow, _thread_slow, "慢线程(温湿度)") < 0) {
        g_run = false;
        return -1;
    }
    g_slow_started = true;

    printf("[bridge] 本地采集已启动（DHT11=%s  MPU6050=%s）\n",
           access(DEV_DHT11,   F_OK) == 0 ? "就绪" : "未找到",
           access(DEV_MPU6050, F_OK) == 0 ? "就绪" : "未找到");
    printf("[bridge] 发动机温度/倒车距离 为预留通道，未接数据源时面板显示\"未接入\"\n");

    return 0;
}

void sensor_bridge_stop(void)
{
    if(!g_run) return;
    g_run = false;

    if(g_fast_started) { pthread_join(g_tid_fast, NULL); g_fast_started = false; }
    if(g_slow_started) { pthread_join(g_tid_slow, NULL); g_slow_started = false; }

    if(g_fd_dht11 >= 0) { close(g_fd_dht11); g_fd_dht11 = -1; }
    if(g_fd_mpu   >= 0) { close(g_fd_mpu);   g_fd_mpu   = -1; }

    printf("[bridge] 采集已停止\n");
}
