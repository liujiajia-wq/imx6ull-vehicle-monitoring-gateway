// 异常处理单元（邮箱任务）
//
// 在邮箱系统里注册的名字： "alarm"
// 启动方式：register_thread_mail_task(pmail, GW_UNIT_ALARM, alarm_unit_task);
//
// 职责：
//   1) recv_mail_msg 收每一拍采样（和显示单元收到的是同一份数据）
//   2) 阈值判定 + 回差滞环
//   3) 等级跳变时驱动声光报警，并 send_mail_msg 一条事件给 "send_data" 上传
//
// 为什么不写在 LVGL 定时器里：蜂鸣器 / sysfs 都是阻塞操作，
// 放在 UI 线程会让画面卡顿；而且上报要按时间顺序，不能依赖 200ms 界面节拍。

#ifndef _GW_ALARM_UNIT_H
#define _GW_ALARM_UNIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ui_sensor.h"

/* 注册给 create_mailbox_system 的任务函数 */
void * alarm_unit_task(void * arg);

/* ============ 硬件输出钩子 ============ */
/* 默认实现直接驱动 units/alarm_hw.c 的 LED / 蜂鸣器。
 * 想换成继电器、三色灯带、Modbus 报警器，改这两个函数的实现即可。 */
void alarm_output_master(gw_level_t worst);                     /* 全局最高等级变化 */
void alarm_output_sensor(gw_sensor_id_t id, gw_level_t level);  /* 单路等级变化 */

#ifdef __cplusplus
}
#endif

#endif
