// 数据显示单元（邮箱消费者，跑在主线程）
//
// 为什么它不是一条独立线程：
//   LVGL 的所有 lv_xxx() 都不是线程安全的，控件只能在主线程里改。
//   所以显示单元用 register_mail_task_only() 只建邮箱不建线程，
//   由主循环每 5ms 调一次 display_unit_pump() 非阻塞地取信。
//
// 其余四个单元（采集/预警/上传/存储）都是独立线程，只有这里特殊。

#ifndef _GW_DISPLAY_UNIT_H
#define _GW_DISPLAY_UNIT_H

#ifdef __cplusplus
extern "C" {
#endif

/* 从 "show_data" 邮箱取消息并刷到控件上。
 * 主循环里调用，绝不阻塞；一次最多处理 16 条，防止积压把画面卡住。 */
void display_unit_pump(void);

#ifdef __cplusplus
}
#endif

#endif
