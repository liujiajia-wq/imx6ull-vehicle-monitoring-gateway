// 数据显示单元 —— 实现
//
//   try_recv_mail_msg("show_data")  ← 采集单元投递的每一拍数据
//          ↓
//   ui_sensor_set_xxx()  写入快照（加锁）
//          ↓
//   LVGL 的 200ms 定时器  → lv_label_set_text / lv_arc_set_value
//
// 注意这里只做"搬运"，不做任何阈值判断 —— 判断归异常处理单元，
// 界面变红是 ui_sensor.c 读快照后按阈值画的，两边判据一致但互不依赖。

#define _GNU_SOURCE

#include "display_unit.h"
#include "gw_msg.h"
#include "ui_sensor.h"

#include <string.h>

/* 一次最多处理多少条。邮箱里如果积压了（比如主线程卡了一下），
 * 处理完这批剩下的下一拍再取，不会因为一次取太多而卡住画面。 */
#define DISPLAY_MAX_PER_TICK    16

void display_unit_pump(void)
{
    struct msgdata d;
    char           from[32];
    int            n = 0;

    while(n++ < DISPLAY_MAX_PER_TICK &&
          try_recv_mail_msg(&d, from, GW_UNIT_DISPLAY) == 0) {

        /* 只认采集单元发来的采样；报警事件（id>100）是给网络/存储的，
         * 万一错投到这里就忽略 */
        switch(d.id) {
        case GW_ID_CABIN:
            ui_sensor_set_cabin(d.temp, d.hum, (sensor_status_t)d.status);
            break;

        case GW_ID_ENGINE:
            ui_sensor_set_engine(d.temp, (sensor_status_t)d.status);
            break;

        case GW_ID_ATTITUDE:
            ui_sensor_set_attitude(d.pitch, d.roll, (sensor_status_t)d.status);
            break;

        case GW_ID_REAR:
            ui_sensor_set_rear(d.dist, d.reverse != 0, (sensor_status_t)d.status);
            break;

        default:
            break;
        }
    }
}
