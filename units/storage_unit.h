// 数据存储单元（邮箱任务）
//
// 在邮箱系统里注册的名字： "save_data"
// 启动方式：register_thread_mail_task(pmail, GW_UNIT_STORE, storage_unit_task);
//
// 收到两类消息：
//   ① id = 1..4       采集单元投递的采样 → 按节流规则写入 sensor_log 表
//   ② id = 101..104   异常单元投递的等级跳变事件 → 立刻写入 alarm_log 表
//
// 为什么要节流：MPU6050 是 20Hz（50ms 一拍）上报的，四路全存一天就是几百万行，
// SD 卡撑不住。所以每类传感器单独配"最小间隔 + 变化量"，两者满足其一才落库。
//
// 为什么报警事件不节流：报警记录是"一条都不能丢"的事件，而且频率天然很低。

#ifndef _GW_STORAGE_UNIT_H
#define _GW_STORAGE_UNIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* 注册给 create_mailbox_system 的任务函数 */
void *   storage_unit_task(void * arg);

/* 统计（调试用） */
uint32_t storage_unit_rows(void);       /* 已落库的采样行数 */
uint32_t storage_unit_alarms(void);     /* 已落库的报警行数 */
uint32_t storage_unit_dropped(void);    /* 因节流被丢弃的行数 */

/* 当前数据库路径；GW_WITH_SQLITE=0 时返回 "（未启用）" */
const char * storage_unit_dbpath(void);

/* 是否真的在落库（编译时检测到 libsqlite3.so 且运行期打开成功） */
bool     storage_unit_active(void);

#ifdef __cplusplus
}
#endif

#endif
