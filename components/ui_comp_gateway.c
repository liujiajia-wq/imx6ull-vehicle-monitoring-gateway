// This file was generated for SquareLine Studio project
// Target: SquareLine Studio 1.5.3 / LVGL 8.3.11
// Component panels for ui_Gateway
//
// 说明：7 个面板的构建函数集中在此，与 SquareLine 的 components/ 组织方式一致。
//       所有控件均为 ui_ 前缀，与 screens/ui_Gateway.c 中的对象一一对应。

#include "../ui.h"

///////////////////// 通用配色（与 ui_Gateway.c 保持一致）/////////////////////

#define GW_C_BG          0xF2F5F8   /* 屏幕底色 */
#define GW_C_CARD        0xFFFFFF   /* 卡片 */
#define GW_C_LINE        0xE3E8EF   /* 描边 / 分隔线 */
#define GW_C_TEXT        0x16212E   /* 主文字 */
#define GW_C_DIM         0x6B7A8D   /* 次级文字 */
#define GW_C_TAG         0x9AA7B6   /* 型号标签 */
#define GW_C_TRACK       0xEDF1F6   /* 仪表轨道 / 水平仪底 */

#define GW_C_OK          0x2E7D32
#define GW_C_OK_BG       0xE8F5E9
#define GW_C_WARN        0xE8890C
#define GW_C_WARN_BG     0xFFF7E3
#define GW_C_ALARM       0xD32F2F
#define GW_C_ALARM_BG    0xFFEBEE

#define GW_C_CABIN       0x1E88E5   /* DHT11   主题色 */
#define GW_C_ENGINE      0xEF6C00   /* DS18B20 主题色 */
#define GW_C_ATTI        0x7E57C2   /* MPU6050 主题色 */
#define GW_C_REAR        0x00897B   /* HC-SR04 主题色 */
#define GW_C_CLOUD       0x1E88E5

/* 字体别名：四个字号由 fonts/ 下四个文件提供
 * 换字体时只改这 4 行，其余代码不用动
 */
#define GW_FONT_BIG      (&ui_font_cn_32)   /* 卡片大号数值 */
#define GW_FONT_MID      (&ui_font_cn_25)   /* 顶栏标题 */
#define GW_FONT_SMALL    (&ui_font_cn_18)   /* 卡片标题、单位 */
#define GW_FONT_TINY     (&ui_font_cn_14)   /* 标签、角标、提示 */

///////////////////// 内部辅助 ////////////////////

/** 标准卡片：白底 + 12px 圆角 + 1px 描边 */
static lv_obj_t * _gw_card(lv_obj_t * parent, lv_coord_t x, lv_coord_t y,
                           lv_coord_t w, lv_coord_t h)
{
    lv_obj_t * obj = lv_obj_create(parent);
    lv_obj_set_width(obj, w);
    lv_obj_set_height(obj, h);
    lv_obj_set_x(obj, x);
    lv_obj_set_y(obj, y);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(obj, lv_color_hex(GW_C_CARD), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(obj, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(obj, lv_color_hex(GW_C_LINE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    return obj;
}

/** 卡片顶部 3px 主题色装饰条 */
static lv_obj_t * _gw_accent(lv_obj_t * parent, uint32_t color)
{
    lv_obj_t * obj = lv_obj_create(parent);
    lv_obj_set_width(obj, 26);
    lv_obj_set_height(obj, 3);
    lv_obj_set_x(obj, 14);
    lv_obj_set_y(obj, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    return obj;
}

/** 小圆点（状态指示） */
static lv_obj_t * _gw_dot(lv_obj_t * parent, lv_coord_t d, uint32_t color)
{
    lv_obj_t * obj = lv_obj_create(parent);
    lv_obj_set_width(obj, d);
    lv_obj_set_height(obj, d);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    return obj;
}

/** 普通文本标签 */
static lv_obj_t * _gw_text(lv_obj_t * parent, const char * text,
                           const lv_font_t * font, uint32_t color)
{
    lv_obj_t * obj = lv_label_create(parent);
    lv_obj_set_width(obj, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(obj, LV_SIZE_CONTENT);  /// 1
    lv_label_set_text(obj, text);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(obj, font, LV_PART_MAIN | LV_STATE_DEFAULT);
    return obj;
}

/** 绝对定位的文本标签 */
static lv_obj_t * _gw_text_at(lv_obj_t * parent, const char * text,
                              const lv_font_t * font, uint32_t color,
                              lv_coord_t x, lv_coord_t y)
{
    lv_obj_t * obj = _gw_text(parent, text, font, color);
    lv_obj_set_x(obj, x);
    lv_obj_set_y(obj, y);
    return obj;
}

/** 卡片右上角的器件型号标签（右对齐） */
static lv_obj_t * _gw_tag(lv_obj_t * parent, const char * text)
{
    lv_obj_t * obj = _gw_text(parent, text, GW_FONT_TINY, GW_C_TAG);
    lv_obj_align(obj, LV_ALIGN_TOP_RIGHT, -14, 13);
    return obj;
}

/** 左下角状态角标：正常 / 预警 / 超限 / 离线 */
static lv_obj_t * _gw_badge(lv_obj_t * parent)
{
    lv_obj_t * obj = lv_label_create(parent);
    lv_obj_set_width(obj, 62);
    lv_obj_set_height(obj, 22);
    lv_obj_align(obj, LV_ALIGN_BOTTOM_LEFT, 14, -8);
    lv_label_set_text(obj, "离线");
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(obj, lv_color_hex(GW_C_TAG), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(obj, GW_FONT_TINY, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xE9EEF5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(obj, 11, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_ver(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    return obj;
}

/** 卡片底部提示文字（跟在角标右侧） */
static lv_obj_t * _gw_tip(lv_obj_t * parent)
{
    lv_obj_t * obj = _gw_text(parent, "", GW_FONT_TINY, GW_C_DIM);
    lv_obj_align(obj, LV_ALIGN_BOTTOM_LEFT, 84, -11);
    return obj;
}

/** 270° 圆弧仪表 */
static lv_obj_t * _gw_arc(lv_obj_t * parent, int32_t max, uint32_t color)
{
    lv_obj_t * obj = lv_arc_create(parent);
    lv_obj_set_width(obj, 96);
    lv_obj_set_height(obj, 96);
    lv_obj_align(obj, LV_ALIGN_RIGHT_MID, -12, 4);
    lv_arc_set_range(obj, 0, max);
    lv_arc_set_value(obj, 0);
    lv_arc_set_bg_angles(obj, 135, 45);
    lv_obj_remove_style(obj, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    lv_obj_set_style_arc_width(obj, 9, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(obj, lv_color_hex(GW_C_TRACK), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_rounded(obj, false, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(obj, 9, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(obj, lv_color_hex(color), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_rounded(obj, true, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    return obj;
}

/** 水平进度条 */
static lv_obj_t * _gw_bar(lv_obj_t * parent, lv_coord_t w, int32_t max, uint32_t color)
{
    lv_obj_t * obj = lv_bar_create(parent);
    lv_obj_set_width(obj, w);
    lv_obj_set_height(obj, 8);
    lv_obj_align(obj, LV_ALIGN_BOTTOM_MID, 0, -38);
    lv_bar_set_range(obj, 0, max);
    lv_bar_set_value(obj, 0, LV_ANIM_OFF);
    lv_obj_set_style_radius(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, lv_color_hex(GW_C_TRACK), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(obj, 4, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    return obj;
}

///////////////////// TOP BAR ////////////////////

void ui_PanelTopBar_create(void)
{
    ui_PanelTopBar = lv_obj_create(ui_Gateway);
    lv_obj_set_width(ui_PanelTopBar, UI_HOR_RES - 2 * L_PAD);
    lv_obj_set_height(ui_PanelTopBar, L_TOP_H);
    lv_obj_set_x(ui_PanelTopBar, L_X_LEFT);
    lv_obj_set_y(ui_PanelTopBar, L_Y_TOP);
    lv_obj_clear_flag(ui_PanelTopBar, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_opa(ui_PanelTopBar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_PanelTopBar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_PanelTopBar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_LabelTitle = _gw_text_at(ui_PanelTopBar, "车载网关监测", GW_FONT_MID, GW_C_TEXT, 4, 6);

    ui_LabelDevId = _gw_text(ui_PanelTopBar, "i.MX6ULL", GW_FONT_TINY, GW_C_TAG);
    lv_obj_align_to(ui_LabelDevId, ui_LabelTitle, LV_ALIGN_OUT_RIGHT_MID, 8, 3);

    /* 右侧信息按"从右往左"依次贴靠，字体宽度变了也不会重叠 */
    ui_LabelUptime = _gw_text(ui_PanelTopBar, "运行 00:00:00", GW_FONT_TINY, GW_C_TAG);
    lv_obj_align(ui_LabelUptime, LV_ALIGN_RIGHT_MID, -4, 0);

    ui_LabelTxCount = _gw_text(ui_PanelTopBar, "上报 0 条", GW_FONT_TINY, GW_C_DIM);
    lv_obj_align_to(ui_LabelTxCount, ui_LabelUptime, LV_ALIGN_OUT_LEFT_MID, -18, 0);

    ui_LabelNetState = _gw_text(ui_PanelTopBar, "云端离线", GW_FONT_TINY, GW_C_DIM);
    lv_obj_align_to(ui_LabelNetState, ui_LabelTxCount, LV_ALIGN_OUT_LEFT_MID, -18, 0);

    ui_PanelNetDot = _gw_dot(ui_PanelTopBar, 8, GW_C_TAG);
    lv_obj_align_to(ui_PanelNetDot, ui_LabelNetState, LV_ALIGN_OUT_LEFT_MID, -6, 0);
}

///////////////////// ALARM BANNER ////////////////////

void ui_PanelAlarmBanner_create(void)
{
    ui_PanelAlarmBanner = lv_obj_create(ui_Gateway);
    lv_obj_set_width(ui_PanelAlarmBanner, UI_HOR_RES - 2 * L_PAD);
    lv_obj_set_height(ui_PanelAlarmBanner, L_BAN_H);
    lv_obj_set_x(ui_PanelAlarmBanner, L_X_LEFT);
    lv_obj_set_y(ui_PanelAlarmBanner, L_Y_BAN);
    lv_obj_clear_flag(ui_PanelAlarmBanner, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_PanelAlarmBanner, lv_color_hex(GW_C_OK_BG), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelAlarmBanner, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_PanelAlarmBanner, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_PanelAlarmBanner, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_PanelAlarmBanner, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_PanelAlarmDot = _gw_dot(ui_PanelAlarmBanner, 8, GW_C_OK);
    lv_obj_align(ui_PanelAlarmDot, LV_ALIGN_LEFT_MID, 12, 0);

    ui_LabelAlarmText = _gw_text(ui_PanelAlarmBanner, "系统正常 · 4 路传感器在线", GW_FONT_TINY, GW_C_OK);
    lv_obj_align(ui_LabelAlarmText, LV_ALIGN_LEFT_MID, 28, 1);
}

///////////////////// CABIN TEMP / HUMI (DHT11) ////////////////////

void ui_PanelCabin_create(void)
{
    ui_PanelCabin = _gw_card(ui_Gateway, L_X_LEFT, L_Y_BODY, L_CARD_W, L_CARD_H);
    ui_PanelCabinAccent = _gw_accent(ui_PanelCabin, GW_C_CABIN);

    ui_LabelCabinTitle = _gw_text_at(ui_PanelCabin, "驾驶舱温度", GW_FONT_SMALL, GW_C_TEXT, 14, 9);
    ui_LabelCabinTag   = _gw_tag(ui_PanelCabin, "DHT11");

    ui_LabelCabinValue  = _gw_text_at(ui_PanelCabin, "--", GW_FONT_BIG, GW_C_TEXT, 13, 36);
    ui_LabelCabinSymbol = _gw_text(ui_PanelCabin, "°C", GW_FONT_SMALL, GW_C_DIM);
    lv_obj_align_to(ui_LabelCabinSymbol, ui_LabelCabinValue, LV_ALIGN_OUT_RIGHT_MID, 4, 10);

    ui_ArcCabin = _gw_arc(ui_PanelCabin, 1000, GW_C_CABIN);

    ui_LabelCabinBadge = _gw_badge(ui_PanelCabin);
    ui_LabelCabinTip   = _gw_tip(ui_PanelCabin);
    lv_label_set_text(ui_LabelCabinTip, "湿度 --%RH  超限≥38°");
}

///////////////////// ENGINE TEMP (DS18B20) ////////////////////

void ui_PanelEngine_create(void)
{
    ui_PanelEngine = _gw_card(ui_Gateway, L_X_RIGHT, L_Y_BODY, L_CARD_W, L_CARD_H);
    ui_PanelEngineAccent = _gw_accent(ui_PanelEngine, GW_C_ENGINE);

    ui_LabelEngineTitle = _gw_text_at(ui_PanelEngine, "发动机温度", GW_FONT_SMALL, GW_C_TEXT, 14, 9);
    ui_LabelEngineTag   = _gw_tag(ui_PanelEngine, "DS18B20");

    ui_LabelEngineValue  = _gw_text_at(ui_PanelEngine, "--", GW_FONT_BIG, GW_C_TEXT, 13, 36);
    ui_LabelEngineSymbol = _gw_text(ui_PanelEngine, "°C", GW_FONT_SMALL, GW_C_DIM);
    lv_obj_align_to(ui_LabelEngineSymbol, ui_LabelEngineValue, LV_ALIGN_OUT_RIGHT_MID, 4, 10);

    ui_ArcEngine = _gw_arc(ui_PanelEngine, 1000, GW_C_ENGINE);

    ui_LabelEngineBadge = _gw_badge(ui_PanelEngine);
    ui_LabelEngineTip   = _gw_tip(ui_PanelEngine);
    lv_label_set_text(ui_LabelEngineTip, "预警≥95°  超限≥105°");
}

///////////////////// ATTITUDE (MPU6050) ////////////////////

void ui_PanelAttitude_create(void)
{
    ui_PanelAttitude = _gw_card(ui_Gateway, L_X_LEFT, L_Y_ROW2, L_CARD_W, L_CARD_H);
    ui_PanelAttitudeAccent = _gw_accent(ui_PanelAttitude, GW_C_ATTI);

    ui_LabelAttitudeTitle = _gw_text_at(ui_PanelAttitude, "车身姿态", GW_FONT_SMALL, GW_C_TEXT, 14, 9);
    ui_LabelAttitudeTag   = _gw_tag(ui_PanelAttitude, "MPU6050");

    ui_LabelAttitudeValue  = _gw_text_at(ui_PanelAttitude, "--", GW_FONT_BIG, GW_C_TEXT, 13, 36);
    ui_LabelAttitudeSymbol = _gw_text(ui_PanelAttitude, "°", GW_FONT_SMALL, GW_C_DIM);
    lv_obj_align_to(ui_LabelAttitudeSymbol, ui_LabelAttitudeValue, LV_ALIGN_OUT_RIGHT_MID, 4, 10);

    ui_LabelPitchValue = _gw_text_at(ui_PanelAttitude, "俯仰 --", GW_FONT_TINY, GW_C_DIM, 14, 88);
    ui_LabelRollValue  = _gw_text_at(ui_PanelAttitude, "横滚 --", GW_FONT_TINY, GW_C_DIM, 14, 106);

    /* 水平仪：外圈 + 十字线 + 中心参考圆 + 气泡 */
    ui_PanelLevel = lv_obj_create(ui_PanelAttitude);
    lv_obj_set_width(ui_PanelLevel, 96);
    lv_obj_set_height(ui_PanelLevel, 96);
    lv_obj_align(ui_PanelLevel, LV_ALIGN_RIGHT_MID, -12, 4);
    lv_obj_clear_flag(ui_PanelLevel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_PanelLevel, lv_color_hex(GW_C_TRACK), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelLevel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_PanelLevel, LV_RADIUS_CIRCLE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_PanelLevel, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_PanelLevel, lv_color_hex(GW_C_LINE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_PanelLevel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_PanelLevelH = lv_obj_create(ui_PanelLevel);
    lv_obj_set_width(ui_PanelLevelH, 76);
    lv_obj_set_height(ui_PanelLevelH, 2);
    lv_obj_set_x(ui_PanelLevelH, 10);
    lv_obj_set_y(ui_PanelLevelH, 47);
    lv_obj_clear_flag(ui_PanelLevelH, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_PanelLevelH, lv_color_hex(0xC7D0DB), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelLevelH, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_PanelLevelH, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_PanelLevelH, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_PanelLevelH, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_PanelLevelV = lv_obj_create(ui_PanelLevel);
    lv_obj_set_width(ui_PanelLevelV, 2);
    lv_obj_set_height(ui_PanelLevelV, 76);
    lv_obj_set_x(ui_PanelLevelV, 47);
    lv_obj_set_y(ui_PanelLevelV, 10);
    lv_obj_clear_flag(ui_PanelLevelV, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_PanelLevelV, lv_color_hex(0xC7D0DB), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelLevelV, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_PanelLevelV, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_PanelLevelV, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_PanelLevelV, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_PanelLevelRef = lv_obj_create(ui_PanelLevel);
    lv_obj_set_width(ui_PanelLevelRef, 28);
    lv_obj_set_height(ui_PanelLevelRef, 28);
    lv_obj_set_x(ui_PanelLevelRef, 34);
    lv_obj_set_y(ui_PanelLevelRef, 34);
    lv_obj_clear_flag(ui_PanelLevelRef, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_opa(ui_PanelLevelRef, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_PanelLevelRef, LV_RADIUS_CIRCLE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_PanelLevelRef, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_PanelLevelRef, lv_color_hex(0xCFD8E3), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_PanelLevelRef, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_PanelLevelBubble = _gw_dot(ui_PanelLevel, 14, GW_C_ATTI);
    lv_obj_set_x(ui_PanelLevelBubble, 41);
    lv_obj_set_y(ui_PanelLevelBubble, 41);

    ui_LabelAttitudeBadge = _gw_badge(ui_PanelAttitude);
    ui_LabelAttitudeTip   = _gw_tip(ui_PanelAttitude);
    lv_label_set_text(ui_LabelAttitudeTip, "超限≥25°");
}

///////////////////// REAR DISTANCE (HC-SR04) ////////////////////

void ui_PanelRear_create(void)
{
    ui_PanelRear = _gw_card(ui_Gateway, L_X_RIGHT, L_Y_ROW2, L_CARD_W, L_CARD_H);
    ui_PanelRearAccent = _gw_accent(ui_PanelRear, GW_C_REAR);

    ui_LabelRearTitle = _gw_text_at(ui_PanelRear, "倒车检测", GW_FONT_SMALL, GW_C_TEXT, 14, 9);
    ui_LabelRearTag   = _gw_tag(ui_PanelRear, "HC-SR04");

    ui_LabelRearValue  = _gw_text_at(ui_PanelRear, "--", GW_FONT_BIG, GW_C_TEXT, 13, 36);
    ui_LabelRearSymbol = _gw_text(ui_PanelRear, "m", GW_FONT_SMALL, GW_C_DIM);
    lv_obj_align_to(ui_LabelRearSymbol, ui_LabelRearValue, LV_ALIGN_OUT_RIGHT_MID, 4, 10);

    ui_BarRear = _gw_bar(ui_PanelRear, L_CARD_W - 28, 300, GW_C_REAR);

    ui_LabelRearBadge = _gw_badge(ui_PanelRear);
    ui_LabelRearTip   = _gw_tip(ui_PanelRear);
    lv_label_set_text(ui_LabelRearTip, "非倒挡 · 雷达待机");
}

///////////////////// BOTTOM BAR ////////////////////

void ui_PanelBottomBar_create(void)
{
    ui_PanelBottomBar = lv_obj_create(ui_Gateway);
    lv_obj_set_width(ui_PanelBottomBar, UI_HOR_RES - 2 * L_PAD);
    lv_obj_set_height(ui_PanelBottomBar, L_BOT_H);
    lv_obj_set_x(ui_PanelBottomBar, L_X_LEFT);
    lv_obj_set_y(ui_PanelBottomBar, L_Y_BOT);
    lv_obj_clear_flag(ui_PanelBottomBar, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_opa(ui_PanelBottomBar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_PanelBottomBar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_PanelBottomBar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_PanelAlarmStateDot = _gw_dot(ui_PanelBottomBar, 8, GW_C_OK);
    lv_obj_align(ui_PanelAlarmStateDot, LV_ALIGN_LEFT_MID, 4, 0);

    ui_LabelAlarmState = _gw_text(ui_PanelBottomBar, "无报警", GW_FONT_TINY, GW_C_OK);
    lv_obj_align_to(ui_LabelAlarmState, ui_PanelAlarmStateDot, LV_ALIGN_OUT_RIGHT_MID, 8, 1);

    ui_LabelLastReport = _gw_text(ui_PanelBottomBar, "最近上报 --", GW_FONT_TINY, GW_C_TAG);
    lv_obj_align(ui_LabelLastReport, LV_ALIGN_CENTER, 0, 0);

    ui_LabelRssi = _gw_text(ui_PanelBottomBar, "4G --", GW_FONT_TINY, GW_C_TAG);
    lv_obj_align(ui_LabelRssi, LV_ALIGN_RIGHT_MID, -4, 0);
}
