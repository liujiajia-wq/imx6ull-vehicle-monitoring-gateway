// 传感器桥接层 —— 把内核驱动读到的原始数据转成界面要的物理量
//
// 数据链路：
//   你的 .ko 驱动  →  /dev/xxx_misc  →  本文件（两个采集线程）  →  ui_sensor_set_xxx()
//                                                                        ↓
//                                                          200ms 定时器刷控件（UI 线程）
//
// 线程划分（很重要，不能合成一个）：
//   快线程 50ms   MPU6050 姿态（顺便 200ms 轮询一次倒车预留通道）
//   慢线程 2s     DHT11 温湿度（顺便 1s 轮询一次机温预留通道）
//
// 设备打不开不致命：界面照样起，对应面板显示"离线"（灰色）。
//
// 本工程只覆盖 i.MX6ULL 板载的两路传感（DHT11 + MPU6050）。
// 发动机温度、倒车距离两 panel 保留，但默认没有数据源 → 显示"未接入"。
// 要补数据，重写下面两个弱函数即可。

#ifndef SENSOR_BRIDGE_H
#define SENSOR_BRIDGE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动采集（内部自己起线程，调用后立即返回）
 * @return 0 成功；-1 线程创建失败（设备打不开不算失败）
 * @note  必须在 ui_init() + ui_sensor_init() 之后调用
 */
int  sensor_bridge_start(void);

/** @brief 停止采集并关闭设备（退出前调用） */
void sensor_bridge_stop(void);

/* ==========================================================================
 *  预留通道（弱函数，默认返回 false = 未接入）
 *
 *  在你的 .c 里重写同名函数就能给这两块面板供数，签名不能改：
 *
 *      bool bridge_ext_engine_temp(float * temp_c)
 *      {
 *          *temp_c = 92.5f;
 *          return true;
 *      }
 *
 *  返回 false 表示这一拍没有有效数据，面板保持"未接入"。
 * ========================================================================== */

/** @brief 发动机温度 ℃，采集线程 1s 调一次 */
bool bridge_ext_engine_temp(float * temp_c);

/** @brief 倒车距离 m + 倒挡状态，采集线程 200ms 调一次 */
bool bridge_ext_rear_distance(float * dist_m, bool * reverse);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_BRIDGE_H */
