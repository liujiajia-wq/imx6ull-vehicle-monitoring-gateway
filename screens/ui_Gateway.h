// This file was generated for SquareLine Studio project
// Target: SquareLine Studio 1.5.3 / LVGL 8.3.11
// Project name: Gateway
// Screen: Gateway
//
// 说明：
//   1) 本文件只负责"长什么样"（控件创建），不负责数据刷新；
//   2) 所有控件以 ui_ 前缀导出，数据桥接层 ui_sensor.c 直接引用这些对象；
//   3) 布局按 800x480 计算，若换分辨率只需改 ui.c 里的 UI_HOR_RES / UI_VER_RES。

#ifndef _UI_GATEWAY_H
#define _UI_GATEWAY_H

#ifdef __cplusplus
extern "C" {
#endif

///////////////////// LAYOUT（改这两行即可适配其它分辨率）//////////////////////

#define UI_HOR_RES      800
#define UI_VER_RES      480

#define L_PAD           8       /* 屏幕四周边距 */
#define L_TOP_H         38      /* 顶栏高 */
#define L_BAN_H         28      /* 报警横幅高 */
#define L_BOT_H         34      /* 底栏高 */
#define L_GAP           8       /* 卡片间距 */

#define L_CARD_W        ((UI_HOR_RES - 2 * L_PAD - L_GAP) / 2)                      /* 388 */
#define L_CARD_H        ((UI_VER_RES - 2 * L_PAD - L_TOP_H - 6 - L_BAN_H - L_GAP \
                          - L_BOT_H - 2 * L_GAP) / 2)                               /* 167 */

#define L_Y_TOP         L_PAD                                                       /* 8   */
#define L_Y_BAN         (L_Y_TOP + L_TOP_H + 6)                                     /* 52  */
#define L_Y_BODY        (L_Y_BAN + L_BAN_H + L_GAP)                                 /* 88  */
#define L_Y_BOT         (UI_VER_RES - L_PAD - L_BOT_H)                              /* 438 */

#define L_X_LEFT        L_PAD                                                       /* 8   */
#define L_X_RIGHT       (L_PAD + L_CARD_W + L_GAP)                                  /* 404 */
#define L_Y_ROW2        (L_Y_BODY + L_CARD_H + L_GAP)                               /* 263 */

///////////////////// SCREEN ////////////////////

extern lv_obj_t * ui_Gateway;

///////////////////// TOP BAR ////////////////////

extern lv_obj_t * ui_PanelTopBar;
extern lv_obj_t * ui_LabelTitle;
extern lv_obj_t * ui_LabelDevId;
extern lv_obj_t * ui_PanelNetDot;
extern lv_obj_t * ui_LabelNetState;
extern lv_obj_t * ui_LabelTxCount;
extern lv_obj_t * ui_LabelUptime;

///////////////////// ALARM BANNER ////////////////////

extern lv_obj_t * ui_PanelAlarmBanner;
extern lv_obj_t * ui_PanelAlarmDot;
extern lv_obj_t * ui_LabelAlarmText;

///////////////////// CABIN TEMP / HUMI (DHT11) ////////////////

extern lv_obj_t * ui_PanelCabin;
extern lv_obj_t * ui_PanelCabinAccent;
extern lv_obj_t * ui_LabelCabinTitle;
extern lv_obj_t * ui_LabelCabinTag;
extern lv_obj_t * ui_LabelCabinValue;
extern lv_obj_t * ui_LabelCabinSymbol;
extern lv_obj_t * ui_ArcCabin;
extern lv_obj_t * ui_LabelCabinBadge;
extern lv_obj_t * ui_LabelCabinTip;

///////////////////// ENGINE TEMP (DS18B20) ///////////////////

extern lv_obj_t * ui_PanelEngine;
extern lv_obj_t * ui_PanelEngineAccent;
extern lv_obj_t * ui_LabelEngineTitle;
extern lv_obj_t * ui_LabelEngineTag;
extern lv_obj_t * ui_LabelEngineValue;
extern lv_obj_t * ui_LabelEngineSymbol;
extern lv_obj_t * ui_ArcEngine;
extern lv_obj_t * ui_LabelEngineBadge;
extern lv_obj_t * ui_LabelEngineTip;

///////////////////// ATTITUDE (MPU6050) ////////////////////

extern lv_obj_t * ui_PanelAttitude;
extern lv_obj_t * ui_PanelAttitudeAccent;
extern lv_obj_t * ui_LabelAttitudeTitle;
extern lv_obj_t * ui_LabelAttitudeTag;
extern lv_obj_t * ui_LabelAttitudeValue;
extern lv_obj_t * ui_LabelAttitudeSymbol;
extern lv_obj_t * ui_PanelLevel;
extern lv_obj_t * ui_PanelLevelH;
extern lv_obj_t * ui_PanelLevelV;
extern lv_obj_t * ui_PanelLevelRef;
extern lv_obj_t * ui_PanelLevelBubble;
extern lv_obj_t * ui_LabelPitchValue;
extern lv_obj_t * ui_LabelRollValue;
extern lv_obj_t * ui_LabelAttitudeBadge;
extern lv_obj_t * ui_LabelAttitudeTip;

///////////////////// REAR DISTANCE (HC-SR04) ////////////////////

extern lv_obj_t * ui_PanelRear;
extern lv_obj_t * ui_PanelRearAccent;
extern lv_obj_t * ui_LabelRearTitle;
extern lv_obj_t * ui_LabelRearTag;
extern lv_obj_t * ui_LabelRearValue;
extern lv_obj_t * ui_LabelRearSymbol;
extern lv_obj_t * ui_BarRear;
extern lv_obj_t * ui_LabelRearBadge;
extern lv_obj_t * ui_LabelRearTip;

///////////////////// BOTTOM BAR ////////////////////

extern lv_obj_t * ui_PanelBottomBar;
extern lv_obj_t * ui_PanelAlarmStateDot;
extern lv_obj_t * ui_LabelAlarmState;
extern lv_obj_t * ui_LabelLastReport;
extern lv_obj_t * ui_LabelRssi;

///////////////////// BUILD FUNCTIONS ////////////////////

void ui_Gateway_screen_init(void);
void ui_Gateway_screen_destroy(void);

void ui_PanelTopBar_create(void);
void ui_PanelAlarmBanner_create(void);
void ui_PanelCabin_create(void);
void ui_PanelEngine_create(void);
void ui_PanelAttitude_create(void);
void ui_PanelRear_create(void);
void ui_PanelBottomBar_create(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
