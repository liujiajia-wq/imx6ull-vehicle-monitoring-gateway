// 传感器自检工具 —— 不依赖 LVGL / framebuffer，只读两个 /dev 节点
//
// 用途：屏幕上数字乱跳时，先用它判断问题出在"读传感器"还是"画界面"。
//   它算出来的值和 LVGL 界面完全一致（同一套公式、同一套校准），
//   所以两边对照就能定位：
//     · 这里数值正常、屏幕上乱  → 问题在 LVGL/显示层
//     · 这里数值就乱            → 问题在驱动/传感器/接线
//
// 编译（不用改 Makefile，单独一条命令）：
//   arm-linux-gnueabihf-gcc -O2 -std=gnu11 -Wall tools/sensor_dump.c -o sensor_dump -lm
// 板子上运行：
//   insmod dht11_platform_drv.ko ; insmod mpu6050_drv.ko
//   ./sensor_dump
//
// 输出示例：
//   [dht11] raw 2d 00 1a 00  →  温度 26.0 ℃  湿度 45.0 %RH
//   [mpu  ] ax=  420 ay= -7600 az=  3190 gx= -255 gy=    0 | |a|=1.01g | P=  +0.0 R=  +0.0
//   （DHT11 那行 raw 后 4 个十六进制数只要一直稳定，说明驱动解码没问题）

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <time.h>

#define DEV_DHT11       "/dev/dht11_misc"
#define DEV_MPU6050     "/dev/mpu6050_misc"

#define MPU_ACCEL_LSB_PER_G     8192.0f     /* ±4g */
#define MPU_GYRO_LSB_PER_DPS    65.5f       /* ±500°/s */
#define MPU_COMP_ALPHA          0.98f

#define PERIOD_MS_MPU           50
#define PERIOD_MS_DHT11         2000
#define CAL_SAMPLES             60          /* 50ms × 60 ≈ 3 秒 */
#define STILL_TOL_G             0.15f
#define VIB_TOL_G               0.30f

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

/* ---------------- MPU6050 ---------------- */

static bool mpu_read_raw(int fd, int16_t * ax, int16_t * ay, int16_t * az,
                         int16_t * gx, int16_t * gy, int16_t * gz)
{
    unsigned char d[14];
    ssize_t n = read(fd, d, sizeof(d));

    if(n != (ssize_t)sizeof(d)) return false;

    *ax = (int16_t)(((uint16_t)d[0]  << 8) | d[1]);
    *ay = (int16_t)(((uint16_t)d[2]  << 8) | d[3]);
    *az = (int16_t)(((uint16_t)d[4]  << 8) | d[5]);
    /* d[6..7] 是片内温度，这里不用 */
    *gx = (int16_t)(((uint16_t)d[8]  << 8) | d[9]);
    *gy = (int16_t)(((uint16_t)d[10] << 8) | d[11]);
    *gz = (int16_t)(((uint16_t)d[12] << 8) | d[13]);
    return true;
}

/* ---------------- DHT11 ---------------- */

static bool dht11_read_raw(int fd, unsigned char * d4)
{
    ssize_t n = read(fd, d4, 4);
    return n == 4;
}

/* ---------------- main ---------------- */

int main(void)
{
    int fd_dht = open(DEV_DHT11, O_RDWR);
    int fd_mpu = open(DEV_MPU6050, O_RDWR);
    int cal_n = 0;
    double cal_gx = 0.0, cal_gy = 0.0, cal_p = 0.0, cal_r = 0.0;
    float bias_gx = 0.0f, bias_gy = 0.0f, zero_p = 0.0f, zero_r = 0.0f;
    bool cal_done = false, att_inited = false;
    float pitch = 0.0f, roll = 0.0f;
    uint64_t last_us = 0;
    uint32_t next_mpu = 0, next_dht = 0, cal_deadline = 0;
    int fail_mpu = 0, fail_dht = 0;

    if(fd_mpu < 0) fprintf(stderr, "打不开 %s: %s\n", DEV_MPU6050, strerror(errno));
    if(fd_dht < 0) fprintf(stderr, "打不开 %s: %s\n", DEV_DHT11,   strerror(errno));
    if(fd_mpu < 0 && fd_dht < 0) return 1;

    printf("按 Ctrl+C 退出；启动后 3 秒内别动板子（正在测陀螺零偏和零点）\n");
    fflush(stdout);

    cal_deadline = now_ms() + 20000;

    for(;;) {
        uint32_t t = now_ms();

        /* ---- 姿态：50ms 一拍 ---- */
        if(fd_mpu >= 0 && (int32_t)(t - next_mpu) >= 0) {
            int16_t ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;

            next_mpu = t + PERIOD_MS_MPU;

            if(!mpu_read_raw(fd_mpu, &ax, &ay, &az, &gx, &gy, &gz)) {
                if(++fail_mpu >= 3) {
                    printf("[mpu  ] 读取失败 errno=%d %s\n", errno, strerror(errno));
                    fflush(stdout);
                }
                continue;
            }
            fail_mpu = 0;

            {
                float ax_g = (float)ax / MPU_ACCEL_LSB_PER_G;
                float ay_g = (float)ay / MPU_ACCEL_LSB_PER_G;
                float az_g = (float)az / MPU_ACCEL_LSB_PER_G;
                float gx_dps = (float)gx / MPU_GYRO_LSB_PER_DPS;
                float gy_dps = (float)gy / MPU_GYRO_LSB_PER_DPS;
                float amag = sqrtf(ax_g * ax_g + ay_g * ay_g + az_g * az_g);
                float p_acc = atan2f(-ax_g, sqrtf(ay_g * ay_g + az_g * az_g)) * 57.29578f;
                float r_acc = atan2f(ay_g, az_g) * 57.29578f;
                float dt, acc_w;
                uint64_t t_us;

                if(!cal_done) {
                    if(fabsf(amag - 1.0f) < STILL_TOL_G) {
                        cal_gx += gx_dps;
                        cal_gy += gy_dps;
                        cal_p  += p_acc;
                        cal_r  += r_acc;
                        cal_n++;
                    }
                    if(cal_n >= CAL_SAMPLES || (int32_t)(now_ms() - cal_deadline) >= 0) {
                        if(cal_n > 0) {
                            bias_gx = (float)(cal_gx / cal_n);
                            bias_gy = (float)(cal_gy / cal_n);
                            zero_p  = (float)(cal_p / cal_n);
                            zero_r  = (float)(cal_r / cal_n);
                        }
                        cal_done = true;
                        printf("[cal  ] 陀螺零偏 gx=%+.2f gy=%+.2f °/s，零点 P=%+.1f R=%+.1f °（%d 样本）\n",
                               (double)bias_gx, (double)bias_gy, (double)zero_p, (double)zero_r, cal_n);
                        fflush(stdout);
                    }
                    pitch = p_acc - zero_p;
                    roll  = r_acc - zero_r;
                }
                else {
                    gx_dps -= bias_gx;
                    gy_dps -= bias_gy;

                    t_us = now_us();
                    if(!att_inited) {
                        pitch = p_acc;
                        roll  = r_acc;
                        att_inited = true;
                        last_us = t_us;
                    }
                    else {
                        dt = (float)(t_us - last_us) / 1000000.0f;
                        last_us = t_us;
                        if(dt <= 0.0f || dt > 0.5f) dt = PERIOD_MS_MPU / 1000.0f;

                        acc_w = (fabsf(amag - 1.0f) < VIB_TOL_G) ? (1.0f - MPU_COMP_ALPHA) : 0.0f;
                        pitch = (1.0f - acc_w) * (pitch + gy_dps * dt) + acc_w * p_acc;
                        roll  = (1.0f - acc_w) * (roll  + gx_dps * dt) + acc_w * r_acc;
                    }
                    pitch -= zero_p;
                    roll  -= zero_r;
                }

                /* 250ms 打一次，免得刷屏 */
                {
                    static uint32_t next_print = 0;
                    if((int32_t)(t - next_print) >= 0) {
                        next_print = t + 250;
                        printf("[mpu  ] ax=%6d ay=%7d az=%6d gx=%6d gy=%6d gz=%6d | "
                               "|a|=%.2fg | p_acc=%+6.1f r_acc=%+6.1f | P=%+6.1f R=%+6.1f%s\n",
                               ax, ay, az, gx, gy, gz, (double)amag,
                               (double)p_acc, (double)r_acc,
                               (double)pitch, (double)roll,
                               cal_done ? "" : " <校准中>");
                        fflush(stdout);
                    }
                }
            }
        }

        /* ---- 温湿度：2s 一拍 ---- */
        if(fd_dht >= 0 && (int32_t)(t - next_dht) >= 0) {
            unsigned char d[4] = { 0 };

            next_dht = t + PERIOD_MS_DHT11;

            if(!dht11_read_raw(fd_dht, d)) {
                fail_dht++;
                printf("[dht11] 读取失败 errno=%d %s（连续 %d 次）\n",
                       errno, strerror(errno), fail_dht);
            }
            else {
                fail_dht = 0;
                printf("[dht11] raw %02x %02x %02x %02x  →  温度 %.1f ℃  湿度 %.1f %%RH\n",
                       d[0], d[1], d[2], d[3],
                       (double)((float)d[2] + (float)d[3] * 0.1f),
                       (double)((float)d[0] + (float)d[1] * 0.1f));
            }
            fflush(stdout);
        }

        usleep(2000);
    }

    return 0;
}
