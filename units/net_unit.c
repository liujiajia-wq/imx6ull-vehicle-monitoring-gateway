// 网络通信单元 —— 邮箱任务实现
//
//   recv_mail_msg() ← 采集单元 / 异常单元
//          ↓
//   采样？→ 更新本地最新值，满 5s 打包一条遥测上报
//   事件？→ 立刻上报
//          ↓
//   net_send_json()  → 云平台
//          ↓
//   ui_sensor_set_cloud()  → 主线程把"云端在线/离线、上报 N 条"画到顶栏
//
// 注意：本线程绝不调用任何 lv_xxx()。顶栏状态是写进快照，由 UI 线程去画。

#define _GNU_SOURCE

#include "net_unit.h"
#include "gw_msg.h"
#include "ui_sensor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

/* ==========================================================================
 *  可调参数
 * ========================================================================== */

#define NET_TELEMETRY_MS        5000u   /* 遥测上报周期 */
#define NET_SEND_TIMEOUT_MS     3000u   /* 单次发送超时 */
#define NET_ONLINE_WINDOW_MS    15000u  /* 多久没发成功就判离线 */
#define NET_DEBUG_LOG           1

/* ==========================================================================
 *  内部状态
 * ========================================================================== */

static uint32_t        g_tx    = 0;
static uint32_t        g_fail  = 0;
static struct timeval  g_t0;            /* 进程启动时刻，算 uptime 用 */

/* 四路最新值，遥测上报时打包用 */
static struct msgdata  s_last[GW_SENSOR_CNT];
static int             s_have[GW_SENSOR_CNT];

static const char * const s_key[GW_SENSOR_CNT] = {
    "cabin", "engine", "attitude", "rear"
};

static uint32_t _now_ms(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (uint32_t)((uint32_t)tv.tv_sec * 1000u + (uint32_t)tv.tv_usec / 1000u);
}

/* ==========================================================================
 *  JSON 拼装
 * ========================================================================== */

static int _build_telemetry(char * out, size_t n)
{
    return snprintf(out, n,
        "{\"dev\":\"imx6ull-gw\",\"type\":\"telemetry\",\"uptime\":%u,"
        "\"cabin\":{\"t\":%.1f,\"h\":%.0f,\"ok\":%d},"
        "\"engine\":{\"t\":%.1f,\"ok\":%d},"
        "\"att\":{\"p\":%.1f,\"r\":%.1f,\"ok\":%d},"
        "\"rear\":{\"d\":%.2f,\"rev\":%d,\"ok\":%d}}",
        (unsigned)(_now_ms() / 1000u),
        (double)s_last[0].temp, (double)s_last[0].hum, s_have[0],
        (double)s_last[1].temp, s_have[1],
        (double)s_last[2].pitch, (double)s_last[2].roll, s_have[2],
        (double)s_last[3].dist, s_last[3].reverse, s_have[3]);
}

static int _build_alarm(char * out, size_t n, const struct msgdata * d)
{
    static const char * const lv_txt[4] = { "na", "ok", "warn", "alarm" };
    int idx = d->id - GW_ID_ALARM_BASE - 1;
    const char * lv = (d->status >= 0 && d->status < 4) ? lv_txt[d->status] : "na";

    if(idx < 0 || idx >= GW_SENSOR_CNT) idx = 0;

    return snprintf(out, n,
        "{\"dev\":\"imx6ull-gw\",\"type\":\"alarm\","
        "\"sensor\":\"%s\",\"level\":\"%s\",\"value\":%.2f,"
        "\"time\":\"%04d-%02d-%02d %02d:%02d:%02d\"}",
        s_key[idx], lv, (double)d->temp,
        d->tim.tm_year + 1900, d->tim.tm_mon + 1, d->tim.tm_mday,
        d->tim.tm_hour, d->tim.tm_min, d->tim.tm_sec);
}

/* ==========================================================================
 *  发送
 * ========================================================================== */

static uint32_t g_last_ok = 0;

static void _send(const char * json, uint32_t len)
{
    if(net_send_json(json, len, NET_SEND_TIMEOUT_MS)) {
        g_tx++;
        g_last_ok = _now_ms();
#if NET_DEBUG_LOG
        printf("[net] ↑ %u 字节：%.90s%s\n", (unsigned)len, json,
               len > 90u ? " ..." : "");
        fflush(stdout);
#endif
    }
    else {
        g_fail++;
    }
}

static void _refresh_cloud_status(void)
{
    uint32_t now = _now_ms();
    bool     online = (g_last_ok != 0u) && ((now - g_last_ok) < NET_ONLINE_WINDOW_MS);
    uint32_t last_s = (g_last_ok == 0u) ? 0u : ((now - g_last_ok) / 1000u);

    /* 只写快照，不碰控件；rssi 暂无来源写 -127（未知） */
    ui_sensor_set_cloud(online, g_tx, last_s, -127, now / 1000u);
}

/* ==========================================================================
 *  处理一条消息
 * ========================================================================== */

static void _on_sample(const struct msgdata * d)
{
    int idx = d->id - GW_ID_CABIN;

    if(idx >= 0 && idx < GW_SENSOR_CNT) {
        s_last[idx] = *d;
        s_have[idx] = (d->status == GW_ST_OK) ? 1 : 0;
    }
}

void * net_unit_task(void * arg)
{
    struct msgdata d;
    char           from[32];
    char           buf[512];
    uint32_t       next_tel;
    uint32_t       next_ui;

    (void)arg;

    gettimeofday(&g_t0, NULL);
    next_tel = _now_ms();
    next_ui  = next_tel;

    printf("[net] 网络通信单元已启动（遥测 %us 一次，报警事件即时上报）\n",
           (unsigned)(NET_TELEMETRY_MS / 1000u));
    fflush(stdout);

    while(1) {
        uint32_t now;

        memset(&d, 0, sizeof(d));

        /* 阻塞收信；没有消息就一直等在这里，不占 CPU */
        if(recv_mail_msg(&d, from) != 0) {
            usleep(1000);
            continue;
        }

        if(d.id == MSG_ID_QUIT) break;

        if(d.id > GW_ID_ALARM_BASE) {
            /* 报警事件：立刻上报 */
            int len = _build_alarm(buf, sizeof(buf), &d);

            if(len > 0 && (size_t)len < sizeof(buf)) {
                _send(buf, (uint32_t)len);
            }
        }
        else {
            _on_sample(&d);
        }

        /* 周期性遥测：靠消息到达驱动，不需要额外定时线程。
         * 采样 200ms 一拍，所以这个判断 5 秒内必然会被执行到。 */
        now = _now_ms();
        if((int32_t)(now - next_tel) >= 0) {
            int len;

            next_tel = now + NET_TELEMETRY_MS;
            len = _build_telemetry(buf, sizeof(buf));
            if(len > 0 && (size_t)len < sizeof(buf)) {
                _send(buf, (uint32_t)len);
            }
        }

        /* 每秒刷一次顶栏状态 */
        if((int32_t)(now - next_ui) >= 0) {
            next_ui = now + 1000u;
            _refresh_cloud_status();
        }
    }

    printf("[net] 网络通信单元已退出（成功 %u 条，失败 %u 条）\n",
           (unsigned)g_tx, (unsigned)g_fail);
    return NULL;
}

uint32_t net_unit_tx_count(void)
{
    return g_tx;
}

uint32_t net_unit_fail_count(void)
{
    return g_fail;
}

/* ==========================================================================
 *  默认发送实现：未接入云平台
 * ========================================================================== */

__attribute__((weak)) bool net_send_json(const char * json, uint32_t len, uint32_t timeout_ms)
{
    static uint32_t n = 0;

    (void)json;
    (void)timeout_ms;

    n++;
    /* 别刷屏：前 2 条打一次，之后每 60 条打一次 */
    if(n <= 2u || (n % 60u) == 0u) {
        fprintf(stderr, "[net] 云平台未接入（重写 net_send_json 即可），"
                        "第 %u 条被丢弃，%u 字节\n", (unsigned)n, (unsigned)len);
    }
    return false;
}
