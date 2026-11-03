// This file was generated for SquareLine Studio project
// Target: SquareLine Studio 1.5.3 / LVGL 8.3.11
// Screen: Gateway (大型车辆车载网关监测)
// Resolution: 800x480
//
// 说明：本文件只定义对象指针与页面生命周期，
//       具体控件构建见 components/ui_comp_gateway.c
//       数据刷新见 ui_sensor.c

#include "../ui.h"

lv_obj_t * ui_Gateway = NULL;

lv_obj_t * ui_PanelTopBar = NULL;
lv_obj_t * ui_LabelTitle = NULL;
lv_obj_t * ui_LabelDevId = NULL;
lv_obj_t * ui_PanelNetDot = NULL;
lv_obj_t * ui_LabelNetState = NULL;
lv_obj_t * ui_LabelTxCount = NULL;
lv_obj_t * ui_LabelUptime = NULL;

lv_obj_t * ui_PanelAlarmBanner = NULL;
lv_obj_t * ui_PanelAlarmDot = NULL;
lv_obj_t * ui_LabelAlarmText = NULL;

lv_obj_t * ui_PanelCabin = NULL;
lv_obj_t * ui_PanelCabinAccent = NULL;
lv_obj_t * ui_LabelCabinTitle = NULL;
lv_obj_t * ui_LabelCabinTag = NULL;
lv_obj_t * ui_LabelCabinValue = NULL;
lv_obj_t * ui_LabelCabinSymbol = NULL;
lv_obj_t * ui_ArcCabin = NULL;
lv_obj_t * ui_LabelCabinBadge = NULL;
lv_obj_t * ui_LabelCabinTip = NULL;

lv_obj_t * ui_PanelEngine = NULL;
lv_obj_t * ui_PanelEngineAccent = NULL;
lv_obj_t * ui_LabelEngineTitle = NULL;
lv_obj_t * ui_LabelEngineTag = NULL;
lv_obj_t * ui_LabelEngineValue = NULL;
lv_obj_t * ui_LabelEngineSymbol = NULL;
lv_obj_t * ui_ArcEngine = NULL;
lv_obj_t * ui_LabelEngineBadge = NULL;
lv_obj_t * ui_LabelEngineTip = NULL;

lv_obj_t * ui_PanelAttitude = NULL;
lv_obj_t * ui_PanelAttitudeAccent = NULL;
lv_obj_t * ui_LabelAttitudeTitle = NULL;
lv_obj_t * ui_LabelAttitudeTag = NULL;
lv_obj_t * ui_LabelAttitudeValue = NULL;
lv_obj_t * ui_LabelAttitudeSymbol = NULL;
lv_obj_t * ui_PanelLevel = NULL;
lv_obj_t * ui_PanelLevelH = NULL;
lv_obj_t * ui_PanelLevelV = NULL;
lv_obj_t * ui_PanelLevelRef = NULL;
lv_obj_t * ui_PanelLevelBubble = NULL;
lv_obj_t * ui_LabelPitchValue = NULL;
lv_obj_t * ui_LabelRollValue = NULL;
lv_obj_t * ui_LabelAttitudeBadge = NULL;
lv_obj_t * ui_LabelAttitudeTip = NULL;

lv_obj_t * ui_PanelRear = NULL;
lv_obj_t * ui_PanelRearAccent = NULL;
lv_obj_t * ui_LabelRearTitle = NULL;
lv_obj_t * ui_LabelRearTag = NULL;
lv_obj_t * ui_LabelRearValue = NULL;
lv_obj_t * ui_LabelRearSymbol = NULL;
lv_obj_t * ui_BarRear = NULL;
lv_obj_t * ui_LabelRearBadge = NULL;
lv_obj_t * ui_LabelRearTip = NULL;

lv_obj_t * ui_PanelBottomBar = NULL;
lv_obj_t * ui_PanelAlarmStateDot = NULL;
lv_obj_t * ui_LabelAlarmState = NULL;
lv_obj_t * ui_LabelLastReport = NULL;
lv_obj_t * ui_LabelRssi = NULL;

// event funtions

// build funtions

void ui_Gateway_screen_init(void)
{
    ui_Gateway = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Gateway, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_Gateway, lv_color_hex(0xF2F5F8), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Gateway, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_Gateway, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_Gateway, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_PanelTopBar_create();
    ui_PanelAlarmBanner_create();
    ui_PanelCabin_create();
    ui_PanelEngine_create();
    ui_PanelAttitude_create();
    ui_PanelRear_create();
    ui_PanelBottomBar_create();
}

void ui_Gateway_screen_destroy(void)
{
    if(ui_Gateway) lv_obj_del(ui_Gateway);

    // NULL screen variables
    ui_Gateway = NULL;

    ui_PanelTopBar = NULL;
    ui_LabelTitle = NULL;
    ui_LabelDevId = NULL;
    ui_PanelNetDot = NULL;
    ui_LabelNetState = NULL;
    ui_LabelTxCount = NULL;
    ui_LabelUptime = NULL;

    ui_PanelAlarmBanner = NULL;
    ui_PanelAlarmDot = NULL;
    ui_LabelAlarmText = NULL;

    ui_PanelCabin = NULL;
    ui_PanelCabinAccent = NULL;
    ui_LabelCabinTitle = NULL;
    ui_LabelCabinTag = NULL;
    ui_LabelCabinValue = NULL;
    ui_LabelCabinSymbol = NULL;
    ui_ArcCabin = NULL;
    ui_LabelCabinBadge = NULL;
    ui_LabelCabinTip = NULL;

    ui_PanelEngine = NULL;
    ui_PanelEngineAccent = NULL;
    ui_LabelEngineTitle = NULL;
    ui_LabelEngineTag = NULL;
    ui_LabelEngineValue = NULL;
    ui_LabelEngineSymbol = NULL;
    ui_ArcEngine = NULL;
    ui_LabelEngineBadge = NULL;
    ui_LabelEngineTip = NULL;

    ui_PanelAttitude = NULL;
    ui_PanelAttitudeAccent = NULL;
    ui_LabelAttitudeTitle = NULL;
    ui_LabelAttitudeTag = NULL;
    ui_LabelAttitudeValue = NULL;
    ui_LabelAttitudeSymbol = NULL;
    ui_PanelLevel = NULL;
    ui_PanelLevelH = NULL;
    ui_PanelLevelV = NULL;
    ui_PanelLevelRef = NULL;
    ui_PanelLevelBubble = NULL;
    ui_LabelPitchValue = NULL;
    ui_LabelRollValue = NULL;
    ui_LabelAttitudeBadge = NULL;
    ui_LabelAttitudeTip = NULL;

    ui_PanelRear = NULL;
    ui_PanelRearAccent = NULL;
    ui_LabelRearTitle = NULL;
    ui_LabelRearTag = NULL;
    ui_LabelRearValue = NULL;
    ui_LabelRearSymbol = NULL;
    ui_BarRear = NULL;
    ui_LabelRearBadge = NULL;
    ui_LabelRearTip = NULL;

    ui_PanelBottomBar = NULL;
    ui_PanelAlarmStateDot = NULL;
    ui_LabelAlarmState = NULL;
    ui_LabelLastReport = NULL;
    ui_LabelRssi = NULL;
}
