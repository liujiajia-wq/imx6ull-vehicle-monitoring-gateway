// This file was generated for SquareLine Studio project
// Target: SquareLine Studio 1.5.3 / LVGL 8.3.11
// Project name: Gateway
//
// 大型车辆车载网关监测系统 —— 主界面入口
// 结构与本工程模板一致：ui.c/ui.h + screens/ + components/ + 数据桥接层 ui_sensor.*

#ifndef _GATEWAY_UI_H
#define _GATEWAY_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/lvgl.h"
#include <stdbool.h>

#include "ui_helpers.h"
#include "ui_events.h"

///////////////////// SCREENS ////////////////////

#include "screens/ui_Gateway.h"

///////////////////// VARIABLES ////////////////////

// EVENTS
extern lv_obj_t * ui____initial_actions0;

// FONTS
// 四档中文字号，由 fonts/ 下四个文件提供（lv_font_conv 生成，见 fonts/ 里的说明）
LV_FONT_DECLARE(ui_font_cn_14);
LV_FONT_DECLARE(ui_font_cn_18);
LV_FONT_DECLARE(ui_font_cn_25);
LV_FONT_DECLARE(ui_font_cn_32);

// UI INIT
void ui_init(void);
void ui_destroy(void);

///////////////////// 数据桥接 ////////////////////
// 采集线程 → ui_sensor_set_xxx() → 200ms 定时器 → 刷新控件
void ui_sensor_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
