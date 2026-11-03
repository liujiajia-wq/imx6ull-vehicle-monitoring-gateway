// This file was generated for SquareLine Studio project
// Target: SquareLine Studio 1.5.3 / LVGL 8.3.11
//
// 传感器数据桥接层：把采集线程的数据安全地送进 UI 线程
//
// 用法：
//   1) lv_init() 与 ui_init() 之后调用一次 ui_sensor_init();
//   2) 采集线程调用 ui_sensor_set_xxx() 上报数据（任意线程安全）;
//   3) 主循环按周期调用 lv_timer_handler()。
//
// 线程模型：set_xxx() 只写一份快照（加锁），
//          200ms 定时器在 LVGL 线程里读走快照并刷控件 —— LVGL 全程只在 UI 线程被调用。

#ifndef _UI_SENSOR_H
#define _UI_SENSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

///////////////////// 数据有效性 ////////////////////

typedef enum {
    SENSOR_OK = 0,          /* 数据正常 */
    SENSOR_TIMEOUT,         /* 通信超时 / 器件无响应 */
    SENSOR_INVALID          /* 校验失败 / 数据越界 */
} sensor_status_t;

///////////////////// 传感器编号 ////////////////////

typedef enum {
    GW_CABIN = 0,           /* DHT11    驾驶舱温湿度 */
    GW_ENGINE,              /* DS18B20  发动机温度 */
    GW_ATTITUDE,            /* MPU6050  车身姿态   */
    GW_REAR,                /* HC-SR04  倒车距离   */
    GW_SENSOR_CNT
} gw_sensor_id_t;

///////////////////// 报警等级 ////////////////////

typedef enum {
    GW_LEVEL_NA = 0,        /* 离线 */
    GW_LEVEL_OK,            /* 正常 */
    GW_LEVEL_WARN,          /* 预警：黄 */
    GW_LEVEL_ALARM          /* 超限：红 + 闪烁 + 声光报警 */
} gw_level_t;

///////////////////// 报警阈值 ////////////////////

typedef struct {
    float cabin_warn;       /* 驾驶舱温度预警 ℃ */
    float cabin_alarm;      /* 驾驶舱温度超限 ℃ */
    float engine_warn;      /* 发动机温度预警 ℃ */
    float engine_alarm;     /* 发动机温度超限 ℃ */
    float attitude_warn;    /* 倾角预警 ° */
    float attitude_alarm;   /* 倾角超限 ° */
    float dist_warn;        /* 倒车距离预警 m（小于触发） */
    float dist_alarm;       /* 倒车距离超限 m */
    float hyst_c;           /* 温度回差 ℃ */
    float hyst_deg;         /* 角度回差 ° */
    float hyst_m;           /* 距离回差 m */
} ui_threshold_t;

/* 默认阈值：现场标定后可用 ui_sensor_set_threshold() 覆盖 */
#define GW_TH_CABIN_WARN_C      30.0f
#define GW_TH_CABIN_ALARM_C     38.0f
#define GW_TH_ENGINE_WARN_C     95.0f
#define GW_TH_ENGINE_ALARM_C    105.0f
#define GW_TH_ATTITUDE_WARN_DEG 15.0f
#define GW_TH_ATTITUDE_ALARM_DEG 25.0f
#define GW_TH_DIST_WARN_M       1.00f
#define GW_TH_DIST_ALARM_M      0.50f

/* 倒车雷达量程 m，用于进度条刻度 */
#define GW_REAR_RANGE_M         3.0f

///////////////////// 数据快照 ////////////////////

/* 四路数据 + 云端状态的"最新值"。
 *
 * 这是显示单元的数据源，也是其它单元（网络上报、存储）读最新值的地方。
 * 注意：快照只保存"当前是多少"，不保留历史 —— 要事件请走邮箱队列。
 * 跨线程读取必须用 ui_sensor_copy_snapshot()，不要直接读全局变量。 */
typedef struct {
    /* 驾驶舱温湿度 DHT11 */
    float           cabin_c;
    float           humi_rh;
    sensor_status_t cabin_st;

    /* 发动机温度 DS18B20 */
    float           engine_c;
    sensor_status_t engine_st;

    /* 车身姿态 MPU6050 */
    float           pitch_deg;
    float           roll_deg;
    sensor_status_t att_st;

    /* 倒车距离 HC-SR04 */
    float           dist_m;
    bool            reverse;
    sensor_status_t rear_st;

    /* 云平台 / 系统 */
    bool            cloud_ok;
    uint32_t        tx_count;
    uint32_t        last_tx_s;
    uint32_t        uptime_s;
    int8_t          rssi;
} gw_snapshot_t;

///////////////////// 初始化 ////////////////////

/**
 * @brief 初始化桥接层，创建 200ms 刷新定时器
 * @note  必须在 lv_init() 与 ui_init() 之后、UI 主线程中调用
 */
void ui_sensor_init(void);

void ui_sensor_set_threshold(const ui_threshold_t * thr);
const ui_threshold_t * ui_sensor_get_threshold(void);

///////////////////// 上报接口（采集线程调用）/////////////////////

/* 驾驶舱：DHT11，温湿度一起报（量程 0~50℃ / 20~90%RH） */
void ui_sensor_set_cabin(float temp_c, float humi_rh, sensor_status_t status);
/* 发动机：DS18B20，只报温度（量程 -55~125℃，防水探头） */
void ui_sensor_set_engine(float temp_c, sensor_status_t status);
void ui_sensor_set_attitude(float pitch_deg, float roll_deg, sensor_status_t status);
void ui_sensor_set_rear(float dist_m, bool reverse_active, sensor_status_t status);

/**
 * @brief 云平台 / 网络状态
 * @param connected   4G/WiFi 是否在线
 * @param tx_count    累计成功上报条数
 * @param last_tx_s   距离上次成功上报多少秒
 * @param rssi_dbm    信号强度（-50 强 / -110 弱），未知填 -127
 * @param uptime_s    设备运行时长
 */
void ui_sensor_set_cloud(bool connected, uint32_t tx_count, uint32_t last_tx_s,
                         int8_t rssi_dbm, uint32_t uptime_s);

///////////////////// 查询 ////////////////////

gw_level_t ui_sensor_get_level(gw_sensor_id_t id);

/**
 * @brief 加锁拷一份最新快照（任意线程安全）
 * @note  给网络通信单元、数据存储单元读最新值用；不要在 LVGL 定时器之外碰控件
 */
void ui_sensor_copy_snapshot(gw_snapshot_t * out);

/**
 * @brief 加锁拷一份当前阈值
 * @note  异常处理单元每次判定前取一次，保证和界面用同一套阈值
 */
void ui_sensor_copy_threshold(ui_threshold_t * out);

/**
 * @brief 报警等级变化回调（弱函数，默认空实现）
 * @note  只在等级"跳变"时调用一次，可以直接在这里开关蜂鸣器 / 三色灯
 */
void ui_alarm_hook(gw_sensor_id_t id, gw_level_t level);

///////////////////// 便捷宏 ////////////////////

#define UI_REPORT_DS18B20_FAIL()    ui_sensor_set_engine(0.0f, SENSOR_TIMEOUT)
#define UI_REPORT_DHT11_FAIL()      ui_sensor_set_cabin(0.0f, 0.0f, SENSOR_TIMEOUT)
#define UI_REPORT_MPU6050_FAIL()    ui_sensor_set_attitude(0.0f, 0.0f, SENSOR_TIMEOUT)
#define UI_REPORT_HCSR04_FAIL()     ui_sensor_set_rear(0.0f, false, SENSOR_TIMEOUT)

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
