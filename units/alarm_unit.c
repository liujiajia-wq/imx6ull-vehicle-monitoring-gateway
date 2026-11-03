// 异常处理单元 —— 邮箱任务实现
//
//   recv_mail_msg()  ← 采集单元投递给 "alarm" 的每一拍数据
//          ↓
//   阈值判定 + 回差滞环（和显示层同一套判据）
//          ↓ 等级跳变
//   ① alarm_output_sensor()  单路提示 / 三色灯
//   ② alarm_output_master()  蜂鸣器（看全局最高等级）
//   ③ send_mail_msg() → "send_data"  一条报警事件，交给网络单元上报

#define _GNU_SOURCE

#include "alarm_unit.h"
#include "alarm_hw.h"
#include "gw_msg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>

/* ==========================================================================
 *  可调参数
 * ========================================================================== */

/* 声光输出由 units/alarm_hw.c 负责：
 * 开机自动探测 LED / 蜂鸣器（sysfs、/dev/beep、PWM、GPIO 都支持），
 * 找不到就在启动时用环境变量指定，不必改这里重编。 */
#define ALARM_DEBUG_LOG         1

/* ==========================================================================
 *  内部状态
 * ========================================================================== */

/* 只是本线程用的等级记忆，别的线程不碰，不需要加锁 */
static gw_level_t s_lv[GW_SENSOR_CNT] = { GW_LEVEL_NA, GW_LEVEL_NA, GW_LEVEL_NA, GW_LEVEL_NA };
static gw_level_t s_worst = GW_LEVEL_NA;

#if ALARM_DEBUG_LOG
static const char * const s_name[GW_SENSOR_CNT] = {
    "驾驶舱", "发动机", "车身姿态", "倒车"
};

static const char * _lv_name(gw_level_t lv)
{
    switch(lv) {
    case GW_LEVEL_ALARM: return "超限";
    case GW_LEVEL_WARN:  return "预警";
    case GW_LEVEL_OK:    return "正常";
    default:             return "离线";
    }
}
#endif

/* ==========================================================================
 *  判定
 * ========================================================================== */

static gw_level_t _eval_high(float v, float warn, float alarm, float hyst, gw_level_t prev)
{
    if(v >= alarm) return GW_LEVEL_ALARM;
    if(prev == GW_LEVEL_ALARM && v > alarm - hyst) return GW_LEVEL_ALARM;
    if(v >= warn) return GW_LEVEL_WARN;
    if(prev == GW_LEVEL_WARN && v > warn - hyst) return GW_LEVEL_WARN;
    return GW_LEVEL_OK;
}

static gw_level_t _eval_low(float v, float warn, float alarm, float hyst, gw_level_t prev)
{
    if(v <= alarm) return GW_LEVEL_ALARM;
    if(prev == GW_LEVEL_ALARM && v < alarm + hyst) return GW_LEVEL_ALARM;
    if(v <= warn) return GW_LEVEL_WARN;
    if(prev == GW_LEVEL_WARN && v < warn + hyst) return GW_LEVEL_WARN;
    return GW_LEVEL_OK;
}

/* msgdata.id（1..4）→ 界面用的传感器编号（0..3） */
static int _sensor_index(const struct msgdata * d)
{
    if(d->id < GW_ID_CABIN || d->id > GW_ID_REAR) return -1;
    return d->id - GW_ID_CABIN;
}

/* 这一拍算出来的等级，以及用于上报的"触发值" */
static gw_level_t _level_of(const struct msgdata * d, const ui_threshold_t * th,
                            gw_level_t prev, float * value)
{
    *value = d->temp;

    if(d->status != GW_ST_OK) return GW_LEVEL_NA;

    switch(d->id) {
    case GW_ID_CABIN:
        return _eval_high(d->temp, th->cabin_warn, th->cabin_alarm, th->hyst_c, prev);

    case GW_ID_ENGINE:
        return _eval_high(d->temp, th->engine_warn, th->engine_alarm, th->hyst_c, prev);

    case GW_ID_ATTITUDE: {
        float ap = fabsf(d->pitch);
        float ar = fabsf(d->roll);
        float tilt = (ap > ar) ? ap : ar;

        *value = tilt;
        return _eval_high(tilt, th->attitude_warn, th->attitude_alarm, th->hyst_deg, prev);
    }

    case GW_ID_REAR:
        *value = d->dist;
        if(!d->reverse) return GW_LEVEL_OK;     /* 没挂倒挡只测距，不报警 */
        return _eval_low(d->dist, th->dist_warn, th->dist_alarm, th->hyst_m, prev);

    default:
        return GW_LEVEL_NA;
    }
}

/* ==========================================================================
 *  处理一条消息
 * ========================================================================== */

static void _handle(const struct msgdata * d)
{
    ui_threshold_t th;
    gw_level_t     lv, old;
    struct msgdata ev;
    float          value = 0.0f;
    int            idx;

    idx = _sensor_index(d);
    if(idx < 0) return;

    ui_sensor_copy_threshold(&th);
    if(th.cabin_alarm <= 0.0f) {
        /* 界面还没初始化时给一套默认值兜底 */
        th.cabin_warn    = GW_TH_CABIN_WARN_C;       th.cabin_alarm    = GW_TH_CABIN_ALARM_C;
        th.engine_warn   = GW_TH_ENGINE_WARN_C;      th.engine_alarm   = GW_TH_ENGINE_ALARM_C;
        th.attitude_warn = GW_TH_ATTITUDE_WARN_DEG;  th.attitude_alarm = GW_TH_ATTITUDE_ALARM_DEG;
        th.dist_warn     = GW_TH_DIST_WARN_M;        th.dist_alarm     = GW_TH_DIST_ALARM_M;
        th.hyst_c = 0.5f; th.hyst_deg = 1.0f; th.hyst_m = 0.05f;
    }

    old = s_lv[idx];
    lv  = _level_of(d, &th, old, &value);
    if(lv == old) return;                       /* 没跳变就不做事，避免刷屏 */

    s_lv[idx] = lv;

    /* ① 单路变化 */
    alarm_output_sensor((gw_sensor_id_t)idx, lv);

#if ALARM_DEBUG_LOG
    printf("[alarm] %s %s → %s（当前值 %.2f）\n",
           s_name[idx], _lv_name(old), _lv_name(lv), (double)value);
    fflush(stdout);
#endif

    /* ② 投事件：网络单元立刻上报，存储单元落库 */
    ev        = *d;
    ev.id     = GW_ID_ALARM_BASE + d->id;
    ev.status = (int)lv;
    ev.temp   = value;
    gw_fill_time(&ev);
    send_mail_msg(GW_UNIT_ALARM, ev, GW_UNIT_NET);
    send_mail_msg(GW_UNIT_ALARM, ev, GW_UNIT_STORE);

    /* ③ 全局最高等级 → 蜂鸣器 */
    {
        gw_level_t w = GW_LEVEL_NA;
        int        i;

        for(i = 0; i < GW_SENSOR_CNT; i++) {
            if(s_lv[i] > w) w = s_lv[i];
        }
        if(w != s_worst) {
            s_worst = w;
            alarm_output_master(w);
        }
    }
}

/* ==========================================================================
 *  任务线程
 * ========================================================================== */

void * alarm_unit_task(void * arg)
{
    struct msgdata d;
    char           from[32];

    (void)arg;

    printf("[alarm] 异常处理单元已启动"
           "（舱温≥%.0f/%.0f℃  机温≥%.0f/%.0f℃  倾角≥%.0f/%.0f°）\n",
           (double)GW_TH_CABIN_WARN_C,  (double)GW_TH_CABIN_ALARM_C,
           (double)GW_TH_ENGINE_WARN_C, (double)GW_TH_ENGINE_ALARM_C,
           (double)GW_TH_ATTITUDE_WARN_DEG, (double)GW_TH_ATTITUDE_ALARM_DEG);
    fflush(stdout);

    while(1) {
        memset(&d, 0, sizeof(d));

        if(recv_mail_msg(&d, from) != 0) {
            usleep(1000);
            continue;
        }
        if(d.id == MSG_ID_QUIT) break;          /* 退出消息 */

        _handle(&d);
    }

    alarm_output_master(GW_LEVEL_NA);           /* 退出前关蜂鸣器 */
    printf("[alarm] 异常处理单元已退出\n");
    return NULL;
}

/* ==========================================================================
 *  硬件输出
 *
 *  真正拉电平的活交给 alarm_hw.c（自动探测 + 闪烁状态机），
 *  这里只做两件事：把等级转给它、顺手打一行日志。
 * ========================================================================== */

void alarm_output_master(gw_level_t worst)
{
    alarm_hw_set_level(worst);

#if ALARM_DEBUG_LOG
    printf("[alarm] 全局等级 → %s%s%s\n", _lv_name(worst),
           worst == GW_LEVEL_ALARM ? "（红灯快闪 + 蜂鸣器长鸣）" : "",
           worst == GW_LEVEL_WARN  ? "（黄灯慢闪 + 间歇滴声）"   : "");
    fflush(stdout);
#else
    (void)worst;
#endif
}

void alarm_output_sensor(gw_sensor_id_t id, gw_level_t level)
{
    /* 单路的视觉提示由界面负责，这里预留给三色灯 / 分区指示灯 */
    (void)id;
    (void)level;
}
