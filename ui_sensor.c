// This file was generated for SquareLine Studio project
// Target: SquareLine Studio 1.5.3 / LVGL 8.3.11
//
// 数据桥接层实现：快照 + 互斥锁 + 200ms 刷新定时器
// 负责：数值格式化、阈值判定（含回差）、面板变红/闪烁、报警汇总、云端状态

#include "ui.h"
#include "ui_sensor.h"

#include <stdio.h>
#include <string.h>
#include <pthread.h>

/* 1 = 每秒往串口打一行"UI 线程从快照里拿到了什么、控件上显示的是什么"，
 * 和 bridge 那边的 [mpu]/[dht11] 日志对照，就能判断数据管子通不通。
 * 调通了改 0 即可。 */
#define UI_SENSOR_DEBUG     1

///////////////////// 配色（与 components/ui_comp_gateway.c 一致）/////////////////////

#define COL_TEXT        0x16212E
#define COL_CARD        0xFFFFFF
#define COL_LINE        0xE3E8EF
#define COL_TRACK       0xEDF1F6

#define COL_OK          0x2E7D32
#define COL_OK_BG       0xE8F5E9
#define COL_WARN        0xE8890C
#define COL_WARN_BG     0xFFF7E3
#define COL_ALARM       0xD32F2F
#define COL_ALARM_BG    0xFFEBEE
#define COL_ALARM_BLINK 0xFFCDD2

#define COL_CABIN       0x1E88E5
#define COL_ENGINE      0xEF6C00
#define COL_ATTI        0x7E57C2
#define COL_REAR        0x00897B
#define COL_CLOUD       0x1E88E5

///////////////////// 内部状态 ////////////////////

/* gw_snapshot_t 的定义已经移到 ui_sensor.h，
 * 因为网络单元 / 存储单元也要读这份最新值（它们不 include LVGL）。 */

static gw_snapshot_t    s_snap;
static pthread_mutex_t  s_lock = PTHREAD_MUTEX_INITIALIZER;
static gw_level_t       s_level[GW_SENSOR_CNT];
static bool             s_inited = false;

static ui_threshold_t   s_thr = {
    GW_TH_CABIN_WARN_C,      GW_TH_CABIN_ALARM_C,
    GW_TH_ENGINE_WARN_C,     GW_TH_ENGINE_ALARM_C,
    GW_TH_ATTITUDE_WARN_DEG, GW_TH_ATTITUDE_ALARM_DEG,
    GW_TH_DIST_WARN_M,       GW_TH_DIST_ALARM_M,
    0.5f, 1.0f, 0.05f
};

static const char * const GW_SENSOR_NAME[GW_SENSOR_CNT] = {
    "驾驶舱温度", "发动机温度", "车身姿态", "倒车距离"
};

///////////////////// 小工具 ////////////////////

/** 把任意区间线性映射到圆弧的 0~1000 */
static int32_t _map(float v, float lo, float hi)
{
    float t;
    if(hi <= lo) return 0;
    t = (v - lo) / (hi - lo);
    if(t < 0.0f) t = 0.0f;
    if(t > 1.0f) t = 1.0f;
    return (int32_t)(t * 1000.0f);
}

/** 上限型判定（越大越危险），带 5% 回差防止临界抖动 */
static gw_level_t _eval_high(float v, float warn, float alarm, float hyst, gw_level_t prev)
{
    if(v >= alarm) return GW_LEVEL_ALARM;
    if(prev == GW_LEVEL_ALARM && v > alarm - hyst) return GW_LEVEL_ALARM;
    if(v >= warn) return GW_LEVEL_WARN;
    if(prev == GW_LEVEL_WARN && v > warn - hyst) return GW_LEVEL_WARN;
    return GW_LEVEL_OK;
}

/** 下限型判定（越小越危险），用于倒车距离 */
static gw_level_t _eval_low(float v, float warn, float alarm, float hyst, gw_level_t prev)
{
    if(v <= alarm) return GW_LEVEL_ALARM;
    if(prev == GW_LEVEL_ALARM && v < alarm + hyst) return GW_LEVEL_ALARM;
    if(v <= warn) return GW_LEVEL_WARN;
    if(prev == GW_LEVEL_WARN && v < warn + hyst) return GW_LEVEL_WARN;
    return GW_LEVEL_OK;
}

static uint32_t _accent_of(gw_level_t lv, uint32_t accent)
{
    if(lv == GW_LEVEL_ALARM) return COL_ALARM;
    if(lv == GW_LEVEL_WARN)  return COL_WARN;
    if(lv == GW_LEVEL_NA)    return COL_TRACK;
    return accent;
}

/** 卡片外观随等级变化：正常白底、预警黄边、超限红底并闪烁 */
static void _card_skin(lv_obj_t * card, gw_level_t lv, bool blink)
{
    uint32_t bd = COL_LINE;
    uint32_t bg = COL_CARD;
    lv_coord_t bw = 1;

    if(lv == GW_LEVEL_ALARM) {
        bd = COL_ALARM;
        bw = 2;
        bg = blink ? COL_ALARM_BLINK : COL_ALARM_BG;
    }
    else if(lv == GW_LEVEL_WARN) {
        bd = COL_WARN;
        bw = 2;
        bg = COL_WARN_BG;
    }

    lv_obj_set_style_border_width(card, bw, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(card, lv_color_hex(bd), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(card, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);
}

/** 状态角标 */
static void _badge_set(lv_obj_t * badge, gw_level_t lv)
{
    const char * txt;
    uint32_t bg, fg;

    switch(lv) {
    case GW_LEVEL_ALARM: txt = "超限"; bg = COL_ALARM; fg = 0xFFFFFF; break;
    case GW_LEVEL_WARN:  txt = "预警"; bg = COL_WARN;  fg = 0xFFFFFF; break;
    case GW_LEVEL_OK:    txt = "正常"; bg = COL_OK_BG; fg = COL_OK;   break;
    default:             txt = "离线"; bg = 0xE9EEF5;  fg = 0x9AA7B6; break;
    }

    lv_label_set_text(badge, txt);
    lv_obj_set_style_bg_color(badge, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(badge, lv_color_hex(fg), LV_PART_MAIN | LV_STATE_DEFAULT);
}

/** 等级跳变时回调用户钩子（开关蜂鸣器/三色灯） */
static void _set_level(gw_sensor_id_t id, gw_level_t lv)
{
    if(s_level[id] == lv) return;
    s_level[id] = lv;
    ui_alarm_hook(id, lv);
}

///////////////////// 各卡片刷新 ////////////////////

static void _update_cabin(const gw_snapshot_t * s, bool blink)
{
    gw_level_t lv;
    char buf[64];

    lv = (s->cabin_st == SENSOR_OK)
         ? _eval_high(s->cabin_c, s_thr.cabin_warn, s_thr.cabin_alarm, s_thr.hyst_c, s_level[GW_CABIN])
         : GW_LEVEL_NA;

    if(s->cabin_st == SENSOR_OK) {
        snprintf(buf, sizeof(buf), "%.1f", (double)s->cabin_c);
        lv_label_set_text(ui_LabelCabinValue, buf);
        lv_obj_align_to(ui_LabelCabinSymbol, ui_LabelCabinValue, LV_ALIGN_OUT_RIGHT_MID, 4, 10);
        lv_arc_set_value(ui_ArcCabin, _map(s->cabin_c, 0.0f, 60.0f));
        snprintf(buf, sizeof(buf), "湿度 %.0f%%RH  超限≥%.0f°",
                 (double)s->humi_rh, (double)s_thr.cabin_alarm);
        lv_label_set_text(ui_LabelCabinTip, buf);
    }
    else {
        lv_label_set_text(ui_LabelCabinValue, "--");
        lv_arc_set_value(ui_ArcCabin, 0);
        lv_label_set_text(ui_LabelCabinTip, "DHT11 无响应");
    }

    lv_obj_set_style_text_color(ui_LabelCabinValue,
        lv_color_hex(lv == GW_LEVEL_ALARM ? COL_ALARM : COL_TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(ui_ArcCabin, lv_color_hex(_accent_of(lv, COL_CABIN)),
        LV_PART_INDICATOR | LV_STATE_DEFAULT);

    _card_skin(ui_PanelCabin, lv, blink);
    _badge_set(ui_LabelCabinBadge, lv);
    _set_level(GW_CABIN, lv);
}

static void _update_engine(const gw_snapshot_t * s, bool blink)
{
    gw_level_t lv;
    char buf[64];

    lv = (s->engine_st == SENSOR_OK)
         ? _eval_high(s->engine_c, s_thr.engine_warn, s_thr.engine_alarm, s_thr.hyst_c, s_level[GW_ENGINE])
         : GW_LEVEL_NA;

    if(s->engine_st == SENSOR_OK) {
        snprintf(buf, sizeof(buf), "%.1f", (double)s->engine_c);
        lv_label_set_text(ui_LabelEngineValue, buf);
        lv_obj_align_to(ui_LabelEngineSymbol, ui_LabelEngineValue, LV_ALIGN_OUT_RIGHT_MID, 4, 10);
        lv_arc_set_value(ui_ArcEngine, _map(s->engine_c, 0.0f, 130.0f));
        snprintf(buf, sizeof(buf), "预警≥%.0f°  超限≥%.0f°",
                 (double)s_thr.engine_warn, (double)s_thr.engine_alarm);
        lv_label_set_text(ui_LabelEngineTip, buf);
    }
    else {
        lv_label_set_text(ui_LabelEngineValue, "--");
        lv_arc_set_value(ui_ArcEngine, 0);
        lv_label_set_text(ui_LabelEngineTip, "发动机温度未接入");
    }

    lv_obj_set_style_text_color(ui_LabelEngineValue,
        lv_color_hex(lv == GW_LEVEL_ALARM ? COL_ALARM : COL_TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(ui_ArcEngine, lv_color_hex(_accent_of(lv, COL_ENGINE)),
        LV_PART_INDICATOR | LV_STATE_DEFAULT);

    _card_skin(ui_PanelEngine, lv, blink);
    _badge_set(ui_LabelEngineBadge, lv);
    _set_level(GW_ENGINE, lv);
}

static void _update_attitude(const gw_snapshot_t * s, bool blink)
{
    gw_level_t lv;
    char buf[48];
    float tilt = 0.0f;

    if(s->att_st == SENSOR_OK) {
        float ap = s->pitch_deg < 0 ? -s->pitch_deg : s->pitch_deg;
        float ar = s->roll_deg  < 0 ? -s->roll_deg  : s->roll_deg;
        tilt = (ap > ar) ? ap : ar;
    }

    lv = (s->att_st == SENSOR_OK)
         ? _eval_high(tilt, s_thr.attitude_warn, s_thr.attitude_alarm, s_thr.hyst_deg, s_level[GW_ATTITUDE])
         : GW_LEVEL_NA;

    if(s->att_st == SENSOR_OK) {
        snprintf(buf, sizeof(buf), "%.1f", (double)tilt);
        lv_label_set_text(ui_LabelAttitudeValue, buf);
        lv_obj_align_to(ui_LabelAttitudeSymbol, ui_LabelAttitudeValue, LV_ALIGN_OUT_RIGHT_MID, 4, 10);

        snprintf(buf, sizeof(buf), "俯仰  %+.1f°", (double)s->pitch_deg);
        lv_label_set_text(ui_LabelPitchValue, buf);
        snprintf(buf, sizeof(buf), "横滚  %+.1f°", (double)s->roll_deg);
        lv_label_set_text(ui_LabelRollValue, buf);

        /* 气泡位移：±30° 对应 ±36px（气泡直径 14，容器 96，圆心 48） */
        {
            int32_t dx = (int32_t)(s->roll_deg * 1.2f);
            int32_t dy = (int32_t)(-s->pitch_deg * 1.2f);
            if(dx >  36) dx =  36;
            if(dx < -36) dx = -36;
            if(dy >  36) dy =  36;
            if(dy < -36) dy = -36;
            lv_obj_set_x(ui_PanelLevelBubble, 41 + dx);
            lv_obj_set_y(ui_PanelLevelBubble, 41 + dy);
        }

        snprintf(buf, sizeof(buf), "超限≥%.0f°", (double)s_thr.attitude_alarm);
        lv_label_set_text(ui_LabelAttitudeTip, buf);
    }
    else {
        lv_label_set_text(ui_LabelAttitudeValue, "--");
        lv_label_set_text(ui_LabelPitchValue, "俯仰  --");
        lv_label_set_text(ui_LabelRollValue, "横滚  --");
        lv_obj_set_x(ui_PanelLevelBubble, 41);
        lv_obj_set_y(ui_PanelLevelBubble, 41);
        lv_label_set_text(ui_LabelAttitudeTip, "MPU6050 无应答");
    }

    lv_obj_set_style_text_color(ui_LabelAttitudeValue,
        lv_color_hex(lv == GW_LEVEL_ALARM ? COL_ALARM : COL_TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_PanelLevelBubble, lv_color_hex(_accent_of(lv, COL_ATTI)),
        LV_PART_MAIN | LV_STATE_DEFAULT);

    _card_skin(ui_PanelAttitude, lv, blink);
    _badge_set(ui_LabelAttitudeBadge, lv);
    _set_level(GW_ATTITUDE, lv);
}

static void _update_rear(const gw_snapshot_t * s, bool blink)
{
    gw_level_t lv;
    char buf[64];

    /* 只有挂着倒挡才参与报警，否则就是一个普通测距 */
    if(s->rear_st != SENSOR_OK)          lv = GW_LEVEL_NA;
    else if(!s->reverse)                 lv = GW_LEVEL_OK;
    else lv = _eval_low(s->dist_m, s_thr.dist_warn, s_thr.dist_alarm, s_thr.hyst_m, s_level[GW_REAR]);

    if(s->rear_st == SENSOR_OK) {
        int32_t cm;

        snprintf(buf, sizeof(buf), "%.2f", (double)s->dist_m);
        lv_label_set_text(ui_LabelRearValue, buf);
        lv_obj_align_to(ui_LabelRearSymbol, ui_LabelRearValue, LV_ALIGN_OUT_RIGHT_MID, 4, 10);

        cm = (int32_t)(s->dist_m * 100.0f);
        if(cm > 300) cm = 300;
        if(cm < 0)   cm = 0;
        lv_bar_set_value(ui_BarRear, cm, LV_ANIM_OFF);

        if(s->reverse)
            snprintf(buf, sizeof(buf), "倒车中 · 警戒 ≤%.1fm", (double)s_thr.dist_warn);
        else
            snprintf(buf, sizeof(buf), "非倒挡 · 雷达待机");
        lv_label_set_text(ui_LabelRearTip, buf);
    }
    else {
        lv_label_set_text(ui_LabelRearValue, "--");
        lv_bar_set_value(ui_BarRear, 0, LV_ANIM_OFF);
        lv_label_set_text(ui_LabelRearTip, "倒车雷达未接入");
    }

    lv_obj_set_style_text_color(ui_LabelRearValue,
        lv_color_hex(lv == GW_LEVEL_ALARM ? COL_ALARM : COL_TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_BarRear, lv_color_hex(_accent_of(lv, COL_REAR)),
        LV_PART_INDICATOR | LV_STATE_DEFAULT);

    _card_skin(ui_PanelRear, lv, blink);
    _badge_set(ui_LabelRearBadge, lv);
    _set_level(GW_REAR, lv);
}

///////////////////// 顶栏 / 横幅 / 底栏 ////////////////////

static void _update_cloud(const gw_snapshot_t * s)
{
    char buf[64];

    lv_obj_set_style_bg_color(ui_PanelNetDot,
        lv_color_hex(s->cloud_ok ? COL_CLOUD : 0x9AA7B6), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ui_LabelNetState, s->cloud_ok ? "云端已连接" : "云端离线");

    snprintf(buf, sizeof(buf), "上报 %u 条", (unsigned)s->tx_count);
    lv_label_set_text(ui_LabelTxCount, buf);

    snprintf(buf, sizeof(buf), "运行 %02u:%02u:%02u",
             (unsigned)(s->uptime_s / 3600u),
             (unsigned)((s->uptime_s / 60u) % 60u),
             (unsigned)(s->uptime_s % 60u));
    lv_label_set_text(ui_LabelUptime, buf);

    if(s->cloud_ok) {
        if(s->last_tx_s <= 60u)
            snprintf(buf, sizeof(buf), "最近上报 %u 秒前", (unsigned)s->last_tx_s);
        else
            snprintf(buf, sizeof(buf), "上报超时 %u 秒", (unsigned)s->last_tx_s);
    }
    else {
        snprintf(buf, sizeof(buf), "云端断开 · 本地缓存中");
    }
    lv_label_set_text(ui_LabelLastReport, buf);

    if(s->rssi <= -120)
        snprintf(buf, sizeof(buf), "4G --");
    else
        snprintf(buf, sizeof(buf), "4G %d dBm", (int)s->rssi);
    lv_label_set_text(ui_LabelRssi, buf);
}

static void _update_summary(bool blink)
{
    int n_alarm = 0, n_warn = 0, n_na = 0;
    int first_alarm = -1, first_warn = -1;
    char buf[128];
    int i;

    for(i = 0; i < GW_SENSOR_CNT; i++) {
        if(s_level[i] == GW_LEVEL_ALARM) {
            n_alarm++;
            if(first_alarm < 0) first_alarm = i;
        }
        else if(s_level[i] == GW_LEVEL_WARN) {
            n_warn++;
            if(first_warn < 0) first_warn = i;
        }
        else if(s_level[i] == GW_LEVEL_NA) {
            n_na++;
        }
    }

    /* ---- 报警横幅 ---- */
    if(n_alarm > 0) {
        lv_obj_set_style_bg_color(ui_PanelAlarmBanner,
            lv_color_hex(blink ? COL_ALARM_BLINK : COL_ALARM), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_LabelAlarmText,
            lv_color_hex(blink ? COL_ALARM : 0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_PanelAlarmDot,
            lv_color_hex(blink ? COL_ALARM : 0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

        snprintf(buf, sizeof(buf), "超限报警：%s（共 %d 项）· 已触发声光报警",
                 GW_SENSOR_NAME[first_alarm], n_alarm);
    }
    else if(n_warn > 0) {
        lv_obj_set_style_bg_color(ui_PanelAlarmBanner,
            lv_color_hex(COL_WARN_BG), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_LabelAlarmText,
            lv_color_hex(0xA35C00), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_PanelAlarmDot,
            lv_color_hex(COL_WARN), LV_PART_MAIN | LV_STATE_DEFAULT);

        snprintf(buf, sizeof(buf), "预警：%s（共 %d 项），请注意观察",
                 GW_SENSOR_NAME[first_warn], n_warn);
    }
    else {
        lv_obj_set_style_bg_color(ui_PanelAlarmBanner,
            lv_color_hex(COL_OK_BG), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_LabelAlarmText,
            lv_color_hex(COL_OK), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui_PanelAlarmDot,
            lv_color_hex(COL_OK), LV_PART_MAIN | LV_STATE_DEFAULT);

        if(n_na >= GW_SENSOR_CNT)
            snprintf(buf, sizeof(buf), "传感器未接入 · 等待采集数据");
        else
            snprintf(buf, sizeof(buf), "系统正常 · %d 路传感器在线", GW_SENSOR_CNT - n_na);
    }
    lv_label_set_text(ui_LabelAlarmText, buf);

    /* ---- 底栏报警状态 ---- */
    if(n_alarm > 0) {
        lv_obj_set_style_bg_color(ui_PanelAlarmStateDot,
            lv_color_hex(COL_ALARM), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_LabelAlarmState,
            lv_color_hex(COL_ALARM), LV_PART_MAIN | LV_STATE_DEFAULT);
        snprintf(buf, sizeof(buf), "%d 项超限", n_alarm);
    }
    else if(n_warn > 0) {
        lv_obj_set_style_bg_color(ui_PanelAlarmStateDot,
            lv_color_hex(COL_WARN), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_LabelAlarmState,
            lv_color_hex(COL_WARN), LV_PART_MAIN | LV_STATE_DEFAULT);
        snprintf(buf, sizeof(buf), "%d 项预警", n_warn);
    }
    else {
        lv_obj_set_style_bg_color(ui_PanelAlarmStateDot,
            lv_color_hex(COL_OK), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_LabelAlarmState,
            lv_color_hex(COL_OK), LV_PART_MAIN | LV_STATE_DEFAULT);
        snprintf(buf, sizeof(buf), "无报警");
    }
    lv_label_set_text(ui_LabelAlarmState, buf);
}

///////////////////// 刷新定时器 ////////////////////

static void _refresh_cb(lv_timer_t * t)
{
    gw_snapshot_t s;
    bool blink;

    (void)t;
    if(!s_inited || ui_Gateway == NULL) return;

    pthread_mutex_lock(&s_lock);
    s = s_snap;
    pthread_mutex_unlock(&s_lock);

#if UI_SENSOR_DEBUG
    /* 这条日志回答的就是"LVGL 到底从哪儿拿的数据"：
     * 前半段 = UI 线程刚拷贝出来的快照，后半段 = 控件上现在的文字。
     * 两者对得上，说明管子是通的，问题只可能在显示层。 */
    {
        static uint32_t next_log = 0;
        uint32_t now = lv_tick_get();

        if((int32_t)(now - next_log) >= 0) {
            next_log = now + 1000;
            printf("[ui ] 快照 cabin=%.1f℃/%.0f%%  att=%.1f/%.1f  |  "
                   "控件: 舱='%s' 姿态='%s'\n",
                   (double)s.cabin_c, (double)s.humi_rh,
                   (double)s.pitch_deg, (double)s.roll_deg,
                   ui_LabelCabinValue    ? lv_label_get_text(ui_LabelCabinValue)    : "NULL",
                   ui_LabelAttitudeValue ? lv_label_get_text(ui_LabelAttitudeValue) : "NULL");
            fflush(stdout);
        }
    }
#endif

    /* 500ms 一个相位，报警面板与横幅同步闪烁 */
    blink = ((lv_tick_get() / 500u) & 1u) != 0u;

    _update_cabin(&s, blink);
    _update_engine(&s, blink);
    _update_attitude(&s, blink);
    _update_rear(&s, blink);
    _update_cloud(&s);
    _update_summary(blink);
}

///////////////////// 对外接口 ////////////////////

void ui_sensor_init(void)
{
    int i;

    memset(&s_snap, 0, sizeof(s_snap));
    /* 注意：memset 后 status 字段是 0，而 0 就是 SENSOR_OK，
     * 不显式置成 TIMEOUT 的话，第一帧会先闪一下 0.0 / 0.00 的假数据 */
    s_snap.cabin_st  = SENSOR_TIMEOUT;
    s_snap.engine_st = SENSOR_TIMEOUT;
    s_snap.att_st    = SENSOR_TIMEOUT;
    s_snap.rear_st   = SENSOR_TIMEOUT;

    for(i = 0; i < GW_SENSOR_CNT; i++) s_level[i] = GW_LEVEL_NA;

    s_snap.rssi = -127;
    s_inited = true;

    lv_timer_create(_refresh_cb, 200, NULL);
}

void ui_sensor_set_threshold(const ui_threshold_t * thr)
{
    if(thr == NULL) return;
    pthread_mutex_lock(&s_lock);
    s_thr = *thr;
    pthread_mutex_unlock(&s_lock);
}

const ui_threshold_t * ui_sensor_get_threshold(void)
{
    return &s_thr;
}

void ui_sensor_set_cabin(float temp_c, float humi_rh, sensor_status_t status)
{
    pthread_mutex_lock(&s_lock);
    s_snap.cabin_c  = temp_c;
    s_snap.humi_rh  = humi_rh;
    s_snap.cabin_st = status;
    pthread_mutex_unlock(&s_lock);
}

void ui_sensor_set_engine(float temp_c, sensor_status_t status)
{
    pthread_mutex_lock(&s_lock);
    s_snap.engine_c  = temp_c;
    s_snap.engine_st = status;
    pthread_mutex_unlock(&s_lock);
}

void ui_sensor_set_attitude(float pitch_deg, float roll_deg, sensor_status_t status)
{
    pthread_mutex_lock(&s_lock);
    s_snap.pitch_deg = pitch_deg;
    s_snap.roll_deg  = roll_deg;
    s_snap.att_st    = status;
    pthread_mutex_unlock(&s_lock);
}

void ui_sensor_set_rear(float dist_m, bool reverse_active, sensor_status_t status)
{
    pthread_mutex_lock(&s_lock);
    s_snap.dist_m  = dist_m;
    s_snap.reverse = reverse_active;
    s_snap.rear_st = status;
    pthread_mutex_unlock(&s_lock);
}

void ui_sensor_set_cloud(bool connected, uint32_t tx_count, uint32_t last_tx_s,
                         int8_t rssi_dbm, uint32_t uptime_s)
{
    pthread_mutex_lock(&s_lock);
    s_snap.cloud_ok   = connected;
    s_snap.tx_count   = tx_count;
    s_snap.last_tx_s  = last_tx_s;
    s_snap.rssi       = rssi_dbm;
    s_snap.uptime_s   = uptime_s;
    pthread_mutex_unlock(&s_lock);
}

gw_level_t ui_sensor_get_level(gw_sensor_id_t id)
{
    if(id >= GW_SENSOR_CNT) return GW_LEVEL_NA;
    return s_level[id];
}

void ui_sensor_copy_snapshot(gw_snapshot_t * out)
{
    if(out == NULL) return;

    pthread_mutex_lock(&s_lock);
    *out = s_snap;
    pthread_mutex_unlock(&s_lock);
}

void ui_sensor_copy_threshold(ui_threshold_t * out)
{
    if(out == NULL) return;

    pthread_mutex_lock(&s_lock);
    *out = s_thr;
    pthread_mutex_unlock(&s_lock);
}

/* 弱函数：重写它来接蜂鸣器 / 三色灯 */
__attribute__((weak)) void ui_alarm_hook(gw_sensor_id_t id, gw_level_t level)
{
    (void)id;
    (void)level;
}
