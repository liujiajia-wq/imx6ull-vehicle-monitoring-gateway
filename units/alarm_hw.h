// 声光报警输出（LED + 蜂鸣器）
//
// alarm_unit 判定出"预警 / 超限"后调 alarm_hw_set_level()，
// 由本模块内部的输出线程按等级产生闪烁 / 长鸣，不阻塞报警单元。
//
// 设计要点：
//   1) 开机自动探测可用设备（环境变量 → /sys/class/leds → /dev/beep → PWM → GPIO）
//   2) 探测成功会顺手自检：亮 250ms + 响 250ms，肉眼可见、耳朵可听
//   3) 板子上没有自动找出来的，用环境变量指定路径，不用改代码重编：
//        GW_LED_PATH=/sys/class/leds/sys-led/brightness \
//        GW_BEEP_PATH=/dev/beep ./gateway_ui

#ifndef _GW_ALARM_HW_H
#define _GW_ALARM_HW_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ui_sensor.h"

/* 探测设备 + 自检 + 启动输出线程。返回 0 表示至少有一路可用。 */
int  alarm_hw_init(void);

/* 停线程并关灯 / 关蜂鸣器 */
void alarm_hw_deinit(void);

/* 设置全局最高等级：NA/OK 全灭，WARN 慢闪 + 短鸣，ALARM 快闪 + 长鸣 */
void alarm_hw_set_level(gw_level_t worst);

/* 手动自检一次（不论有没有设 GW_NO_SELFTEST） */
void alarm_hw_test(void);

int  alarm_hw_led_ready(void);
int  alarm_hw_beep_ready(void);
const char * alarm_hw_led_path(void);
const char * alarm_hw_beep_path(void);

#ifdef __cplusplus
}
#endif

#endif
