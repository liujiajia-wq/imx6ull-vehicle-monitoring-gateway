
#ifndef __MAILBOX_H__
#define __MAILBOX_H__

#include "linklist.h"

extern Node_t *create_mailbox_system();
extern int register_thread_mail_task(Node_t *pmail, char *name, void * (*pfun)(void *));
extern void destroy_mailbox(Node_t *pmail);
extern int recv_mail_msg(struct msgdata *mydata, char *send_name);
extern int send_mail_msg(char *send_name, struct msgdata data, char *recv_name);

/* ===================================================================
 *  在原有接口上追加的三个（原有接口一行没改）
 * =================================================================== */

/* 退出消息：收线程看到 id == MSG_ID_QUIT 就 return NULL */
#define MSG_ID_QUIT     (-1)

/* 只建邮箱、不建线程。
 * 用在"显示单元"上：LVGL 必须跑在主线程（lv_xxx 不是线程安全的），
 * 所以显示单元不能是一条独立线程，只能让主线程用 try_recv_mail_msg() 去取。 */
extern int register_mail_task_only(Node_t *pmail, char *name);

/* 非阻塞收信（按名字找队列，不依赖 pthread_self）。
 * 返回值：0=取到一条  -1=队列空  -2=没有这个名字的邮箱 */
extern int try_recv_mail_msg(struct msgdata *mydata, char *send_name, char *my_name);

/* 给所有已注册的单元投一条退出消息，让阻塞在 recv_mail_msg 上的线程返回，
 * 之后 destroy_mailbox() 里的 pthread_join 才不会永久卡住。 */
extern int send_mail_quit(void);

#endif
