// 单元之间的约定：邮箱名字 + msgdata.id 编码
//
// 整套系统只用一个 struct msgdata 传数据，所以"这条消息是什么"
// 全靠 id 字段区分。这里把约定集中写死，避免各单元各写一套对不上。
//
//   id = 1  驾驶舱温湿度   temp=温度℃   hum=湿度%RH
//   id = 2  发动机温度     temp=温度℃
//   id = 3  车身姿态       pitch / roll
//   id = 4  倒车距离       dist / reverse
//
//   id = 101..104        异常单元发出的"等级跳变事件"（= 100 + 传感器id）
//                        status 里放等级 0=离线 1=正常 2=预警 3=超限
//                        temp 里放触发时的数值
//
//   id = -1 (MSG_ID_QUIT)  退出消息

#ifndef _GW_MSG_H
#define _GW_MSG_H

#include "mailbox.h"

#include <string.h>
#include <time.h>

/* 存储单元开关（Makefile 根据 lib/lib/libsqlite3.so 是否存在自动 -D 传进来）。
 * 没定义时按"不启用"处理 —— 工程保持四单元，存储单元不参与编译、不注册邮箱。 */
#ifndef GW_WITH_SQLITE
#define GW_WITH_SQLITE      0
#endif

///////////////////// 传感器编号（写进 msgdata.id）/////////////////////

#define GW_ID_CABIN         1
#define GW_ID_ENGINE        2
#define GW_ID_ATTITUDE      3
#define GW_ID_REAR          4

///////////////////// 报警事件偏移 ////////////////////

#define GW_ID_ALARM_BASE    100

///////////////////// 状态码（写进 msgdata.status）/////////////////////

#define GW_ST_OK            0
#define GW_ST_TIMEOUT       1
#define GW_ST_INVALID       2

///////////////////// 单元名称（邮箱的收件人 / 发件人）/////////////////////

#define GW_UNIT_ACQ         "get_data"
#define GW_UNIT_DISPLAY     "show_data"
#define GW_UNIT_STORE       "save_data"
#define GW_UNIT_NET         "send_data"
#define GW_UNIT_ALARM       "alarm"

////////////////// 小工具 //////////////////

/* 填上本地时间，云平台和数据库都要用 */
static inline void gw_fill_time(struct msgdata * d)
{
    time_t sec = time(NULL);
    struct tm * p = localtime(&sec);

    if(p != NULL) d->tim = *p;
}

/* 采集单元群发：一份给显示、一份给预警、一份给上传、一份给存储。
 *
 * 四个消费者必须都在 main.c 里注册过，否则 send_mail_msg 找不到收件人会返回失败。
 * 注意：一份数据拷四份不会拖慢采集 —— 每次投递只是把 40 字节的 msgdata 压进队列，
 * 各单元线程各自处理，互不阻塞。 */
static inline void gw_post_acq(struct msgdata * d)
{
    gw_fill_time(d);

    send_mail_msg(GW_UNIT_ACQ, *d, GW_UNIT_DISPLAY);
    send_mail_msg(GW_UNIT_ACQ, *d, GW_UNIT_ALARM);
    send_mail_msg(GW_UNIT_ACQ, *d, GW_UNIT_NET);
    send_mail_msg(GW_UNIT_ACQ, *d, GW_UNIT_STORE);
}

#endif
