#include "mailbox.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

Node_t *pmail = NULL;
Node_t *create_mailbox_system()
{
    pmail = CreateEmptyLinkList();
    if (NULL == pmail)
    {
        printf("CreateEmptyLinkList error\n");
        return NULL;
    }

    return pmail;
}

int register_thread_mail_task(Node_t *pmail, char *name, void * (*pfun)(void *))
{
    Node_t *pTmpNode = NULL;

	//1.申请空间
	pTmpNode = malloc(sizeof(Node_t));
	if (NULL == pTmpNode)
	{
		printf("malloc failed\n");
		return -1;
	}
    strcpy(pTmpNode->Data.name, name);
    pTmpNode->Data.pfun = pfun;
    pTmpNode->Data.pque = CreateLinkQueue();
    if (NULL == pTmpNode->Data.pque)
    {
        printf("CreateLinkQueue error\n");
        return -1;
    }

    pthread_mutex_init(&pTmpNode->Data.mutex, NULL);
    sem_init(&pTmpNode->Data.sem, 0, 0);

    pthread_create(&pTmpNode->Data.tid, NULL, pTmpNode->Data.pfun, NULL);

    //3.将pNext赋值
	pTmpNode->pNext = pmail->pNext;

	//4.头节点的pNext指向申请的节点
	pmail->pNext = pTmpNode;

    return 0;
}

void destroy_mailbox(Node_t *pmail)
{

    Node_t *ptmp = pmail->pNext;

    while (ptmp)
    {
        pthread_join(ptmp->Data.tid, NULL);
        DestroyLinkQueue(&ptmp->Data.pque);
        pthread_mutex_destroy(&ptmp->Data.mutex);
        sem_destroy(&ptmp->Data.sem);
        ptmp = ptmp->pNext;
    }

    DestroyLinkList(&pmail);
}

int send_mail_msg(char *send_name, struct msgdata data, char *recv_name)
{
    Node_t *ptmp = pmail->pNext;
    while (ptmp != NULL)
    {
        if (0 == strcmp(ptmp->Data.name, recv_name))
        {
            break;
        }
        ptmp = ptmp->pNext;
    }

    if (NULL == ptmp)
    {
        printf("Not recver : %s\n", recv_name);
        return -1;
    }

    QDataType qdata;
    strcpy(qdata.send_name, send_name);
    strcpy(qdata.recv_name, recv_name);
    qdata.data = data;

    pthread_mutex_lock(&ptmp->Data.mutex);
    EnterLinkQueue(ptmp->Data.pque, qdata);
    sem_post(&ptmp->Data.sem);
    pthread_mutex_unlock(&ptmp->Data.mutex);

    return 0;
}

int recv_mail_msg(struct msgdata *mydata, char *send_name)
{
    pthread_t mytid = pthread_self();
    Node_t *ptmp = pmail->pNext;
    while (ptmp != NULL)
    {
        if (ptmp->Data.tid == mytid)
        {
            break;
        }
        ptmp = ptmp->pNext;
    }
    if (NULL == ptmp)
    {
        printf("Not this thread\n");
        return -1;
    }
    QDataType qdata;

    sem_wait(&ptmp->Data.sem);
    pthread_mutex_lock(&ptmp->Data.mutex);
    int ret = PopLinkQueue(ptmp->Data.pque, &qdata);
    if (ret != 0)
    {
        pthread_mutex_unlock(&ptmp->Data.mutex);
        return -1;
    }
    pthread_mutex_unlock(&ptmp->Data.mutex);

    *mydata = qdata.data;
    strcpy(send_name, qdata.send_name);

    return 0;
}



/* ======================================================================
 *  以下为车载网关工程追加的接口（原有函数一行未改）
 * ====================================================================== */

/* 只建邮箱、不建线程：给"显示单元"用。
 * LVGL 的 lv_xxx() 必须在主线程调用，所以显示单元不能是独立线程，
 * 由主线程用 try_recv_mail_msg() 非阻塞地取。 */
int register_mail_task_only(Node_t *pmail, char *name)
{
    Node_t *pTmpNode = NULL;

    pTmpNode = malloc(sizeof(Node_t));
    if (NULL == pTmpNode)
    {
        printf("malloc failed\n");
        return -1;
    }

    memset(pTmpNode, 0, sizeof(Node_t));
    strcpy(pTmpNode->Data.name, name);
    pTmpNode->Data.tid = 0;
    pTmpNode->Data.pfun = NULL;

    pTmpNode->Data.pque = CreateLinkQueue();
    if (NULL == pTmpNode->Data.pque)
    {
        printf("CreateLinkQueue error\n");
        free(pTmpNode);
        return -1;
    }

    pthread_mutex_init(&pTmpNode->Data.mutex, NULL);
    sem_init(&pTmpNode->Data.sem, 0, 0);

    pTmpNode->pNext = pmail->pNext;
    pmail->pNext = pTmpNode;

    return 0;
}

/* 非阻塞收信：按名字找自己的队列，取不到立刻返回，绝不阻塞主线程。
 * 显示单元在主循环里每 5ms 调一次，有就取，没有就继续画界面。 */
int try_recv_mail_msg(struct msgdata *mydata, char *send_name, char *my_name)
{
    Node_t *ptmp = pmail->pNext;
    QDataType qdata;
    int ret;

    while (ptmp != NULL)
    {
        if (0 == strcmp(ptmp->Data.name, my_name))
        {
            break;
        }
        ptmp = ptmp->pNext;
    }

    if (NULL == ptmp)
    {
        return -2;
    }

    /* 信号量只是"有没有新消息"的提示，这里尝试减 1，减不到也无所谓，
     * 真正的判空以队列为准，避免 sem 与队列状态不一致时卡住。 */
    sem_trywait(&ptmp->Data.sem);

    pthread_mutex_lock(&ptmp->Data.mutex);
    ret = PopLinkQueue(ptmp->Data.pque, &qdata);
    pthread_mutex_unlock(&ptmp->Data.mutex);

    if (ret != 0)
    {
        return -1;
    }

    if (mydata != NULL)
    {
        *mydata = qdata.data;
    }
    if (send_name != NULL)
    {
        strcpy(send_name, qdata.send_name);
    }

    return 0;
}

/* 给每个已注册单元都投一条退出消息。
 * 不清这个场的话，各单元线程永远阻塞在 sem_wait 上，
 * destroy_mailbox() 里的 pthread_join 会一直等下去（Ctrl+C 退不掉）。 */
int send_mail_quit(void)
{
    Node_t *ptmp = pmail->pNext;
    struct msgdata data;

    memset(&data, 0, sizeof(data));
    data.id = MSG_ID_QUIT;

    while (ptmp != NULL)
    {
        send_mail_msg("system", data, ptmp->Data.name);
        ptmp = ptmp->pNext;
    }

    return 0;
}
