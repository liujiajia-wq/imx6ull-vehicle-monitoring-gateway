// 网络通信单元（邮箱任务）
//
// 在邮箱系统里注册的名字： "send_data"
// 启动方式：register_thread_mail_task(pmail, GW_UNIT_NET, net_unit_task);
//
// 收到两类消息：
//   ① id = 1..4       采集单元投递的采样 → 只更新本地最新值，按周期打包上报遥测
//   ② id = 101..104   异常单元投递的等级跳变事件 → 立刻上报，不等周期
//
// 上报内容先拼成 JSON，再交给 net_send_json()。
// 默认实现是"未接入"（返回 false），所以顶栏会一直显示云端离线 —— 这是正常的。
// 接 MQTT / HTTP 时只要重写 net_send_json()，别的地方一行都不用改。

#ifndef _GW_NET_UNIT_H
#define _GW_NET_UNIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* 注册给 create_mailbox_system 的任务函数 */
void *   net_unit_task(void * arg);

uint32_t net_unit_tx_count(void);
uint32_t net_unit_fail_count(void);

/* ============ 发送钩子（弱函数，重写它就接上了云平台） ============
 *
 * json 是已拼好的 UTF-8 字符串，len 是字节数（不含结尾 \0）。
 * 返回 true = 发送成功（顶栏计数 +1、状态置在线），false = 失败。
 *
 * 例：用 paho.mqtt.c
 *   bool net_send_json(const char * json, uint32_t len, uint32_t timeout_ms)
 *   {
 *       return MQTTClient_publish(g_client, "gw/telemetry", (int)len,
 *                                 json, 0, 0, NULL) == MQTTCLIENT_SUCCESS;
 *   }
 */
bool net_send_json(const char * json, uint32_t len, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
