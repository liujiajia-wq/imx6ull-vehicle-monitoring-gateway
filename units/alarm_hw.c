// 声光报警输出实现
//
// 板子上"灯不亮、蜂鸣器不响"通常就三个原因：
//   1) 报警单元只 printf，没有真正操作硬件（干过这事）；
//   2) 写的是猜出来的 sysfs 路径，板子上根本不存在；
//   3) LED 被心跳触发占着（trigger=heartbeat），写 brightness 完全没反应。
// 这里把三件事一次解决：自动探测 → 关掉 trigger → 自检 → 输出线程。
//
// 探测顺序：
//   LED      GW_LED_PATH → /sys/class/leds/*（优先 sys-led / user / led）→ /dev/led
//   蜂鸣器   GW_BEEP_PATH → /dev/beep → /sys/class/leds/*beep* → /sys/class/pwm/* → gpio19
//
// 输出策略（20ms 一拍的状态机）：
//   超限 ALARM : 红灯 250ms 快闪 + 蜂鸣器长鸣（每 3 秒歇 0.4 秒）
//   预警 WARN  : 黄灯 1Hz 慢闪 + 每 2.5 秒"滴"一声
//   正常 / 离线: 全灭

#define _GNU_SOURCE

#include "alarm_hw.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <pthread.h>

#define HW_LOG          1
#define HW_TICK_MS      20

/* 输出后端类型 */
enum {
    DEV_NONE = 0,
    DEV_SYSFS,      /* sysfs 属性文件：brightness / value / enable */
    DEV_CHARDEV     /* 字符设备：/dev/beep 这种，写 "1"/"0" */
};

/* 板子设备树（puteled / myled0 / mybeep0）：
 *   LED    = GPIO1_IO03 → gpio3    节点出自自写驱动，/dev 名字跟 .ko 里 device_create 有关
 *   蜂鸣器 = GPIO5_IO01 → gpio129（SNVS_TAMPER1），mybeep0 直接操作寄存器
 * .ko 加载了就走 /dev，没加载时用 sysfs GPIO 兜底。 */
static const char * const s_led_devs[]  = {
    "/dev/led", "/dev/puteled", "/dev/myled0", "/dev/myled"
};
static const char * const s_beep_devs[] = {
    "/dev/beep", "/dev/mybeep0", "/dev/mybeep", "/dev/putebeep"
};

typedef struct {
    int  kind;
    char path[128];     /* 已确认可写的那个文件的全路径 */
} hw_out_t;

static hw_out_t        s_led;
static hw_out_t        s_beep;

static pthread_t       s_tid;
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static int             s_run;
static gw_level_t      s_target = GW_LEVEL_NA;

/* ==========================================================================
 *  底层写文件
 * ========================================================================== */

static int _wr(const char * path, const char * val)
{
    int     fd;
    ssize_t n;

    if(path == NULL || val == NULL) return -1;

    fd = open(path, O_WRONLY);
    if(fd < 0) return -1;

    n = write(fd, val, strlen(val));
    close(fd);

    return (n == (ssize_t)strlen(val)) ? 0 : -1;
}

static void _raw(hw_out_t * o, int on)
{
    if(o->kind == DEV_NONE || o->path[0] == '\0') return;
    _wr(o->path, on ? "1" : "0");
}

/* ==========================================================================
 *  探测：LED
 * ========================================================================== */

/* /sys/class/leds/<name> 能不能用 */
static int _probe_led_class(const char * name)
{
    char trig[160];
    char bright[160];

    if(name == NULL || name[0] == '\0' || name[0] == '.') return -1;

    snprintf(bright, sizeof(bright), "/sys/class/leds/%s/brightness", name);
    if(access(bright, W_OK) != 0) return -1;

    /* 关键一步：出厂的 sys-led 常常被 heartbeat 触发占用，
     * 此时往 brightness 写什么都没用，必须先 trigger=none。 */
    snprintf(trig, sizeof(trig), "/sys/class/leds/%s/trigger", name);
    _wr(trig, "none");

    if(_wr(bright, "1") != 0) return -1;
    usleep(120 * 1000);
    _wr(bright, "0");

    s_led.kind = DEV_SYSFS;
    snprintf(s_led.path, sizeof(s_led.path), "%s", bright);
    return 0;
}

static int _scan_led_class(void)
{
    static const char * const prefer[] = { "sys-led", "user", "led" };
    DIR *           d;
    struct dirent * e;
    size_t          i;

    d = opendir("/sys/class/leds");
    if(d == NULL) return -1;

    for(i = 0; i < sizeof(prefer) / sizeof(prefer[0]); i++) {
        rewinddir(d);
        while((e = readdir(d)) != NULL) {
            if(strstr(e->d_name, prefer[i]) == NULL) continue;
            if(_probe_led_class(e->d_name) == 0) {
                closedir(d);
                return 0;
            }
        }
    }

    closedir(d);
    return -1;
}

/* ==========================================================================
 *  探测：蜂鸣器
 * ========================================================================== */

static int _probe_chardev(const char * path, hw_out_t * out)
{
    if(_wr(path, "0") != 0) return -1;

    out->kind = DEV_CHARDEV;
    snprintf(out->path, sizeof(out->path), "%s", path);
    return 0;
}

/* 按候选名单逐个试 /dev 节点 */
static int _first_chardev(const char * const * list, size_t n, hw_out_t * out)
{
    size_t i;

    for(i = 0; i < n; i++) {
        if(_probe_chardev(list[i], out) == 0) return 0;
    }
    return -1;
}

/* 有些板子把蜂鸣器也挂在 led class 下面 */
static int _scan_beep_led(void)
{
    static const char * const keys[] = { "beep", "buzzer", "bell" };
    DIR *           d;
    struct dirent * e;
    size_t          i;
    char            f[160];

    d = opendir("/sys/class/leds");
    if(d == NULL) return -1;

    for(i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        rewinddir(d);
        while((e = readdir(d)) != NULL) {
            if(strstr(e->d_name, keys[i]) == NULL) continue;

            snprintf(f, sizeof(f), "/sys/class/leds/%s/brightness", e->d_name);
            if(_wr(f, "0") == 0) {
                s_beep.kind = DEV_SYSFS;
                snprintf(s_beep.path, sizeof(s_beep.path), "%s", f);
                closedir(d);
                return 0;
            }
        }
    }

    closedir(d);
    return -1;
}

/* 无源蜂鸣器接 PWM 时，靠方波发声：period 1ms → 1kHz */
static int _pwm_setup(const char * chip)
{
    char dir[128];
    char f[160];

    snprintf(dir, sizeof(dir), "%s/pwm0", chip);

    if(access(dir, F_OK) != 0) {
        snprintf(f, sizeof(f), "%s/export", chip);
        if(_wr(f, "0") != 0) return -1;
        usleep(80 * 1000);
        if(access(dir, F_OK) != 0) return -1;
    }

    /* 必须先写 period，再写 duty_cycle */
    snprintf(f, sizeof(f), "%s/period", dir);
    if(_wr(f, "1000000") != 0) return -1;

    snprintf(f, sizeof(f), "%s/duty_cycle", dir);
    _wr(f, "500000");

    s_beep.kind = DEV_SYSFS;
    snprintf(s_beep.path, sizeof(s_beep.path), "%s/enable", dir);
    return 0;
}

static int _scan_pwm(void)
{
    DIR *           d;
    struct dirent * e;
    char            chip[128];

    d = opendir("/sys/class/pwm");
    if(d == NULL) return -1;

    while((e = readdir(d)) != NULL) {
        if(strncmp(e->d_name, "pwmchip", 7) != 0) continue;

        snprintf(chip, sizeof(chip), "/sys/class/pwm/%s", e->d_name);
        if(_pwm_setup(chip) == 0) {
            closedir(d);
            return 0;
        }
    }

    closedir(d);
    return -1;
}

/* 最后一招：GPIO 直推（i.MX6ULL 开发板的蜂鸣器多在 GPIO1_IO19） */
static int _gpio_pin(const char * num, hw_out_t * out)
{
    char dir[96];
    char val[96];

    if(access("/sys/class/gpio/export", F_OK) != 0) return -1;

    snprintf(dir, sizeof(dir), "/sys/class/gpio/gpio%s/direction", num);
    if(access(dir, F_OK) != 0) {
        _wr("/sys/class/gpio/export", num);
        usleep(60 * 1000);
    }
    if(access(dir, F_OK) != 0) return -1;

    _wr(dir, "out");

    snprintf(val, sizeof(val), "/sys/class/gpio/gpio%s/value", num);
    if(_wr(val, "0") != 0) return -1;

    out->kind = DEV_SYSFS;
    snprintf(out->path, sizeof(out->path), "%s", val);
    return 0;
}

/* 环境变量里手工指定（最可靠的一招） */
static int _from_env(const char * name, hw_out_t * out)
{
    const char * p = getenv(name);

    if(p == NULL || *p == '\0') return -1;
    if(_wr(p, "0") != 0) return -1;

    out->kind = (strncmp(p, "/dev/", 5) == 0) ? DEV_CHARDEV : DEV_SYSFS;
    snprintf(out->path, sizeof(out->path), "%s", p);
    return 0;
}

/* ==========================================================================
 *  输出线程
 * ========================================================================== */

static void * _hw_thread(void * arg)
{
    unsigned int t = 0;
    int          last_led  = -1;
    int          last_beep = -1;

    (void)arg;

    while(1) {
        gw_level_t lv;
        int        led  = 0;
        int        beep = 0;
        int        run;

        pthread_mutex_lock(&s_lock);
        run = s_run;
        lv  = s_target;
        pthread_mutex_unlock(&s_lock);

        if(!run) break;

        switch(lv) {
        case GW_LEVEL_ALARM:
            /* 想改成常亮，把下一行换成 led = 1; */
            led  = ((t / 250) % 2) == 0;
            beep = ((t % 3000) < 2600);
            break;

        case GW_LEVEL_WARN:
            led  = ((t % 1000) < 300);
            beep = ((t % 2500) < 120);
            break;

        default:
            led  = 0;
            beep = 0;
            break;
        }

        if(led != last_led) {
            _raw(&s_led, led);
            last_led = led;
        }
        if(beep != last_beep) {
            _raw(&s_beep, beep);
            last_beep = beep;
        }

        usleep(HW_TICK_MS * 1000);
        t += HW_TICK_MS;
    }

    _raw(&s_led, 0);
    _raw(&s_beep, 0);
    return NULL;
}

/* 自检：亮一下 + 响一下，用来确认路径选对了 */
static void _pulse(void)
{
    _raw(&s_led, 1);
    _raw(&s_beep, 1);
    usleep(250 * 1000);
    _raw(&s_led, 0);
    _raw(&s_beep, 0);
}

/* ==========================================================================
 *  对外接口
 * ========================================================================== */

int alarm_hw_init(void)
{
    memset(&s_led,  0, sizeof(s_led));
    memset(&s_beep, 0, sizeof(s_beep));

    /* ---- LED ---- */
    if(_from_env("GW_LED_PATH", &s_led) != 0 &&
       _scan_led_class() != 0 &&
       _first_chardev(s_led_devs, sizeof(s_led_devs) / sizeof(s_led_devs[0]), &s_led) != 0 &&
       _gpio_pin("3", &s_led) != 0) {            /* GPIO1_IO03 = gpio3 */
        s_led.kind = DEV_NONE;
        s_led.path[0] = '\0';
    }

    /* ---- 蜂鸣器 ---- */
    if(_from_env("GW_BEEP_PATH", &s_beep) != 0 &&
       _first_chardev(s_beep_devs, sizeof(s_beep_devs) / sizeof(s_beep_devs[0]), &s_beep) != 0 &&
       _scan_beep_led() != 0 &&
       _scan_pwm() != 0 &&
       _gpio_pin("129", &s_beep) != 0) {         /* GPIO5_IO01 = gpio129 */
        s_beep.kind = DEV_NONE;
        s_beep.path[0] = '\0';
    }

#if HW_LOG
    printf("[alarm] 声光输出  LED = %s\n",
           s_led.kind  == DEV_NONE ? "未找到" : s_led.path);
    printf("[alarm] 声光输出  蜂鸣器 = %s\n",
           s_beep.kind == DEV_NONE ? "未找到" : s_beep.path);

    if(s_led.kind == DEV_NONE || s_beep.kind == DEV_NONE) {
        printf("[alarm] 若板上有对应设备却没用上，启动时手工指定即可（不必改代码）：\n"
               "        GW_LED_PATH=/dev/puteled GW_BEEP_PATH=/dev/mybeep0 ./gateway_ui\n");
    }
    fflush(stdout);
#endif

    if(getenv("GW_NO_SELFTEST") == NULL) {
        _pulse();       /* 亮 250ms + 响 250ms */
    }

    pthread_mutex_lock(&s_lock);
    s_target = GW_LEVEL_NA;
    s_run    = 1;
    pthread_mutex_unlock(&s_lock);

    if(pthread_create(&s_tid, NULL, _hw_thread, NULL) != 0) {
        s_run = 0;
        fprintf(stderr, "[alarm] 声光输出线程创建失败\n");
        return -1;
    }

    return (s_led.kind != DEV_NONE || s_beep.kind != DEV_NONE) ? 0 : -1;
}

void alarm_hw_deinit(void)
{
    int was_run;

    pthread_mutex_lock(&s_lock);
    was_run  = s_run;
    s_run    = 0;
    s_target = GW_LEVEL_NA;
    pthread_mutex_unlock(&s_lock);

    if(was_run) {
        pthread_join(s_tid, NULL);
    }

    _raw(&s_led, 0);
    _raw(&s_beep, 0);
}

void alarm_hw_set_level(gw_level_t worst)
{
    pthread_mutex_lock(&s_lock);
    s_target = worst;
    pthread_mutex_unlock(&s_lock);
}

void alarm_hw_test(void)
{
    _pulse();
}

int alarm_hw_led_ready(void)
{
    return s_led.kind != DEV_NONE;
}

int alarm_hw_beep_ready(void)
{
    return s_beep.kind != DEV_NONE;
}

const char * alarm_hw_led_path(void)
{
    return (s_led.kind == DEV_NONE) ? NULL : s_led.path;
}

const char * alarm_hw_beep_path(void)
{
    return (s_beep.kind == DEV_NONE) ? NULL : s_beep.path;
}
