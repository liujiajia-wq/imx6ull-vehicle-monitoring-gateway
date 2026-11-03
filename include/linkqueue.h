#ifndef __LINKQUEUE_H__
#define __LINKQUEUE_H__

#include <time.h>

struct msgdata
{
    int id;           //设备id
    float temp;       //温度
    float  hum;        //湿度
    float  v;          //电压
    float a;           //电流
    //.......
    struct tm tim;   //时间戳

    /* ---- 车载网关新增字段（原有字段含义不变，只在末尾追加） ---- */
    float pitch;      //俯仰角 °    id=2 姿态时有效
    float roll;       //横滚角 °    id=2 姿态时有效
    float dist;       //倒车距离 m  id=4 倒车时有效
    int   status;     //传感器状态 0=正常 1=超时 2=无效
    int   reverse;    //是否挂倒挡 1=倒车中
};

struct msg
{
      char send_name[32];  //邮箱消息的发送者名字
      struct msgdata data;    //共享的数据
      char recv_name[32];  //邮箱消息的接收者名字
};


typedef struct msg QDataType;

typedef struct qnode 
{
	QDataType Data;
	struct qnode *pNext;
}QNode_t;

extern QNode_t *CreateLinkQueue(void);
extern int IsEmptyLinkQueue(QNode_t *pTmpQueue);
extern int EnterLinkQueue(QNode_t *pTmpQueue, QDataType TmpData);
extern int PopLinkQueue(QNode_t *pTmpQueue, QDataType *pTmpData);
extern int DestroyLinkQueue(QNode_t **ppTmpQueue);

#endif
