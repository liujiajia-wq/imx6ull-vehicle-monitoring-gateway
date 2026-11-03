// This file was generated for SquareLine Studio project
// Target: SquareLine Studio 1.5.3 / LVGL 8.3.11
// Project name: Gateway
//
// 主程序：framebuffer 显示 + LVGL 主循环 + 采集线程（示例）
//
// 编译：make             → 交叉编译到 i.MX6ULL
//       make NATIVE=1    → 本机编译调试
// 运行：./gateway_ui     （需要 root 或 /dev/fb0 写权限）

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>

#include "lvgl/lvgl.h"
#include "ui.h"
#include "ui_sensor.h"
#include "sensor_bridge.h"
#include "mailbox.h"        /* 你的线程邮箱：linklist + linkqueue + 命名队列 */
#include "gw_msg.h"         /* 单元名字 / msgdata.id 约定 */
#include "alarm_unit.h"     /* 异常处理单元 */
#include "alarm_hw.h"       /* 声光报警：LED + 蜂鸣器（自动探测 + 闪烁） */
#include "net_unit.h"       /* 网络通信单元 */
#include "storage_unit.h"   /* 数据存储单元：采集数据写入 SQLite */
#include "display_unit.h"   /* 数据显示单元：从邮箱取数据刷控件 */

/* ==========================================================================
 *  可调参数
 * ========================================================================== */
#define FB_DEV              "/dev/fb0"
#define DRAW_BUF_LINES      60

/* 数据来源：
 *   1 = 模拟数据（四块面板全部自动跳，只看界面效果时用）
 *   0 = 真实传感器（当前板子上就这两路）：
 *        DHT11   /dev/dht11_misc    驾驶舱温湿度
 *        MPU6050 /dev/mpu6050_misc  车身姿态
 *       发动机温度、倒车距离两块面板保留，但没有数据源 → 显示"未接入"
 *       （要补，重写 bridge/sensor_bridge.h 里的两个弱函数）
 */
#define DEMO_MODE           0

/* ==========================================================================
 *  framebuffer
 * ========================================================================== */
static int       g_fb_fd  = -1;
static uint8_t * g_fb_mem = NULL;
static size_t    g_fb_size = 0;
static int       g_bpp = 4;
static struct fb_var_screeninfo g_vinfo;
static struct fb_fix_screeninfo g_finfo;
static volatile sig_atomic_t g_quit = 0;

static void _on_signal(int sig)
{
    (void)sig;
    g_quit = 1;
}

static int fbdev_init(void)
{
    g_fb_fd = open(FB_DEV, O_RDWR);
    if(g_fb_fd < 0) {
        fprintf(stderr, "[fb] 打开 %s 失败: %s\n", FB_DEV, strerror(errno));
        return -1;
    }
    if(ioctl(g_fb_fd, FBIOGET_FSCREENINFO, &g_finfo) < 0) {
        perror("[fb] FBIOGET_FSCREENINFO");
        return -1;
    }
    if(ioctl(g_fb_fd, FBIOGET_VSCREENINFO, &g_vinfo) < 0) {
        perror("[fb] FBIOGET_VSCREENINFO");
        return -1;
    }

    g_bpp = g_vinfo.bits_per_pixel / 8;
    g_fb_size = (size_t)g_finfo.line_length * g_vinfo.yres_virtual;

    g_fb_mem = mmap(NULL, g_fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, g_fb_fd, 0);
    if(g_fb_mem == MAP_FAILED) {
        perror("[fb] mmap");
        g_fb_mem = NULL;
        return -1;
    }

    printf("[fb] %ux%u %dbpp  line_length=%u  virtual=%ux%u\n",
           g_vinfo.xres, g_vinfo.yres, g_vinfo.bits_per_pixel,
           g_finfo.line_length, g_vinfo.xres_virtual, g_vinfo.yres_virtual);

    if(g_bpp != 2 && g_bpp != 4) {
        fprintf(stderr, "[fb] 不支持的位深 %d（只处理 16/32bpp）\n", g_bpp * 8);
        return -1;
    }

    memset(g_fb_mem, 0, g_fb_size);
    return 0;
}

static void fbdev_deinit(void)
{
    if(g_fb_mem) { munmap(g_fb_mem, g_fb_size); g_fb_mem = NULL; }
    if(g_fb_fd >= 0) { close(g_fb_fd); g_fb_fd = -1; }
}

static void fbdev_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * px_map)
{
    const int32_t w = lv_area_get_width(area);
    int32_t y, x;

    for(y = area->y1; y <= area->y2; y++) {
        uint8_t * dst = g_fb_mem
                      + (size_t)(y + g_vinfo.yoffset) * g_finfo.line_length
                      + (size_t)(area->x1 + g_vinfo.xoffset) * g_bpp;

        if(g_bpp == 4) {
            uint32_t * d = (uint32_t *)dst;
            for(x = 0; x < w; x++) {
                lv_color_t c = px_map[x];
                d[x] = 0xFF000000u
                     | ((uint32_t)c.ch.red   << g_vinfo.red.offset)
                     | ((uint32_t)c.ch.green << g_vinfo.green.offset)
                     | ((uint32_t)c.ch.blue  << g_vinfo.blue.offset);
            }
        }
        else {
            uint16_t * d = (uint16_t *)dst;
            for(x = 0; x < w; x++) {
                lv_color_t c = px_map[x];
                d[x] = (uint16_t)(((c.ch.red & 0x1F) << 11) |
                                  ((c.ch.green & 0x3F) << 5) |
                                  (c.ch.blue & 0x1F));
            }
        }
        px_map += w;
    }
    lv_disp_flush_ready(drv);
}

static uint32_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

/* ==========================================================================
 *  采集线程（示例）
 *  真实工程里把 _demo_thread 换成你的读取逻辑，
 *  只要调用 ui_sensor_set_xxx() 即可，UI 侧不用改。
 * ========================================================================== */
#if DEMO_MODE
static void * _demo_thread(void * arg)
{
    uint32_t t0 = now_ms();
    uint32_t tx = 0;
    uint32_t last_tx = t0;

    (void)arg;

    while(!g_quit) {
        uint32_t now = now_ms();
        float t = (now - t0) / 1000.0f;

        /* 舱温（DHT11）：慢慢爬升，偶尔越过 30/38 两级；湿度随温度反向变化 */
        ui_sensor_set_cabin(28.0f + 11.0f * sinf(t * 0.07f),
                            58.0f - 12.0f * sinf(t * 0.07f), SENSOR_OK);

        /* 机温（DS18B20）：周期性升到 105 以上，用来验证超限报警 */
        ui_sensor_set_engine(88.0f + 20.0f * sinf(t * 0.15f), SENSOR_OK);

        /* 姿态：横滚摆幅 22°，会触发预警但不到超限 */
        ui_sensor_set_attitude(6.0f * sinf(t * 0.31f),
                               22.0f * sinf(t * 0.13f), SENSOR_OK);

        /* 倒车：一半时间处于倒挡，距离在 1.2~3.2m 之间变化 */
        {
            bool rev = sinf(t * 0.12f) > 0.0f;
            float d = 2.2f + 1.0f * sinf(t * 0.21f);
            ui_sensor_set_rear(d, rev, SENSOR_OK);
        }

        /* 云端：每 30 秒上报一次 */
        if(now - last_tx > 30000u) { last_tx = now; tx += 4; }
        ui_sensor_set_cloud(true, tx, (now - last_tx) / 1000u, -67, (now - t0) / 1000u);

        usleep(200 * 1000);
    }
    return NULL;
}

/* 模拟模式下在屏幕右上角钉一个红底角标。
 * 目的：以后只要 DEMO_MODE=1 编译出来的二进制跑在板子上，一眼就能看出
 * 屏幕上的数是假的，不会再被当成"传感器数据乱跳"。
 * 注意：字库是 127 字子集，没有"模""拟"二字，所以这里用 ASCII + "数据"。 */
static void _demo_watermark(void)
{
    lv_obj_t * lb = lv_label_create(lv_scr_act());

    lv_label_set_text(lb, "DEMO 数据");
    lv_obj_set_style_bg_color(lb, lv_color_hex(0xC0392B), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(lb, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lb, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(lb, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(lb, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(lb, LV_ALIGN_TOP_RIGHT, -10, 46);
}
#endif

/* ==========================================================================
 *  显示单元：从自己的邮箱里取数据
 *
 *  LVGL 不是线程安全的，所以 lv_xxx() 只能在主线程调 ——
 *  显示单元因此不做成独立线程，而是主循环里非阻塞地取自己的队列。
 * ========================================================================== */

static Node_t * g_mail = NULL;

/* ==========================================================================
 *  main
 * ========================================================================== */
int main(void)
{
    static lv_disp_draw_buf_t draw_buf;
    static lv_disp_drv_t      disp_drv;
    lv_color_t * buf1 = NULL;
    size_t px_cnt;

    /* 这行是为了确认板子上跑的是不是刚编译出来的这个二进制：
     * 时间应该和你 make 的时间一致。老二进制没有这行。 */
    printf("[ui] gateway_ui build %s %s\n", __DATE__, __TIME__);
    printf("[ui] 数据源 = %s\n",
           DEMO_MODE ? "模拟波形（面板上的数会自己上下摆，不是传感器）"
                     : "真实传感器 DHT11 + MPU6050");
    fflush(stdout);

    signal(SIGINT,  _on_signal);
    signal(SIGTERM, _on_signal);

    lv_init();

    if(fbdev_init() < 0) return 1;

    px_cnt = (size_t)g_vinfo.xres * DRAW_BUF_LINES;
    buf1 = (lv_color_t *)malloc(px_cnt * sizeof(lv_color_t));
    if(buf1 == NULL) {
        fprintf(stderr, "绘制缓冲申请失败（需要 %zu KB）\n", px_cnt * sizeof(lv_color_t) / 1024);
        return 1;
    }
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, px_cnt);

    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = fbdev_flush;
    disp_drv.hor_res  = g_vinfo.xres;
    disp_drv.ver_res  = g_vinfo.yres;
    lv_disp_drv_register(&disp_drv);

    /* 界面：SquareLine 导出的入口 */
    ui_init();

    /* 数据桥接层：必须在 ui_init() 之后调用（它要绑定到已创建好的控件） */
    ui_sensor_init();

#if DEMO_MODE
    {
        pthread_t tid;

        _demo_watermark();
        if(pthread_create(&tid, NULL, _demo_thread, NULL) != 0) {
            fprintf(stderr, "模拟采集线程创建失败\n");
        }
        else {
            pthread_detach(tid);
        }
    }
#else
    /* ---- 五单元之间的邮箱系统 ----
     *   get_data  采集单元（sensor_bridge 内部两个采集线程，只发不收）
     *   show_data 显示单元（LVGL 必须跑在主线程，所以只建邮箱不建线程）
     *   alarm     异常处理单元
     *   send_data 网络通信单元
     *   save_data 数据存储单元（仅 GW_WITH_SQLITE=1 时注册） */
    g_mail = create_mailbox_system();
    if(g_mail == NULL) {
        fprintf(stderr, "[ui] 邮箱系统创建失败，界面仍可运行（只是没有数据）\n");
    }
    else {
        register_mail_task_only(g_mail, GW_UNIT_DISPLAY);
        register_thread_mail_task(g_mail, GW_UNIT_ALARM, alarm_unit_task);
        register_thread_mail_task(g_mail, GW_UNIT_NET,   net_unit_task);
        register_thread_mail_task(g_mail, GW_UNIT_STORE, storage_unit_task);

        /* 声光报警硬件：先探测好 LED / 蜂鸣器并自检一下（亮 250ms + 响 250ms），
         * 这样上电就能确认接线和路径对不对，不用等真的超限才发现没反应。 */
        alarm_hw_init();

        /* 采集单元：内部起两个线程，读到数据后 send_mail_msg 给三个单元 */
        if(sensor_bridge_start() < 0) {
            fprintf(stderr, "[ui] 本地采集启动失败，界面仍可运行\n");
        }
    }
#endif

    printf("[ui] 启动完成，Ctrl+C 退出\n");

    {
        uint32_t prev = now_ms();
        while(!g_quit) {
            uint32_t t = now_ms();
#if !DEMO_MODE
            display_unit_pump();        /* 显示单元：把邮箱里的数据交给界面 */
#endif
            lv_tick_inc(t - prev);
            prev = t;
            lv_timer_handler();
            usleep(5000);
        }
    }

    printf("\n[ui] 退出\n");
#if !DEMO_MODE
    /* 顺序很重要：
     *   1) 先停采集单元，不再产生新消息
     *   2) 再给各单元投退出消息，让阻塞在 recv_mail_msg 的线程返回
     *   3) 最后销毁邮箱（里面有 pthread_join，线程没退出会一直等） */
    sensor_bridge_stop();
    alarm_hw_deinit();              /* 关灯、关蜂鸣器，别在退出后一直响 */
    if(g_mail != NULL) {
        send_mail_quit();
        destroy_mailbox(g_mail);
        g_mail = NULL;
    }
#endif
    ui_destroy();
    fbdev_deinit();
    free(buf1);
    return 0;
}
