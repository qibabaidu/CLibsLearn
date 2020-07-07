#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "unistd.h"
#include "event.h"
#include "sys/queue.h"
 
#define BUF_MAX_LEN 1024
#define BUF_BODY "hello world\n"

struct msg_item {
    TAILQ_ENTRY(msg_item) next;
    int len;
    char buf[BUF_MAX_LEN];
};
/**
#define TAILQ_HEAD(name, type)						\
struct name {								\
    struct type *tqh_first;			\
    struct type **tqh_last;			\
}
*/
struct msg_queue{
    TAILQ_HEAD(msg_q, msg_item) entries;
    pthread_mutex_t lock;
};

void do_write( void * pArg ) {
    struct msg_queue *pMsgQueue = (struct msg_queue *)pArg;
    while (1){
        struct msg_item *pMsg = (struct msg_item*)malloc(sizeof(struct  msg_item));
        // memset可以方便的清空一个结构类型的变量或数组,等效为将结构体中值置0
        memset(pMsg, 0, sizeof(struct msg_item));

        int nLength = strlen(BUF_BODY);
        strncpy(pMsg->buf,BUF_BODY, nLength);
        pMsg->len = nLength;

        // lock thread
        pthread_mutex_lock(&(pMsgQueue->lock));
        TAILQ_INSERT_TAIL(&(pMsgQueue->entries),pMsg,next);
        pthread_mutex_unlock(&(pMsgQueue->lock));

        sleep(5);
    }
}

void do_read(int fd, short sWhat, void * pArg) {
    struct msg_queue *pMsgQueue = (struct msg_queue *)pArg;
    struct msg_item *pMsg;

    pthread_mutex_lock(&(pMsgQueue->lock));
    while (NULL != (pMsg = TAILQ_FIRST(&(pMsgQueue->entries))))
    {
       printf("Length:%d\tContent: %s\n", pMsg->len, pMsg->buf);
       TAILQ_REMOVE(&(pMsgQueue->entries), pMsg, next);
       free(pMsg);
    }
    pthread_mutex_unlock(&(pMsgQueue->lock));
 
    return ;
}

int main(void) {
    struct event_base *pEventBase = NULL;
    pEventBase = event_base_new();

    struct msg_queue *pMsgQueue = (struct msg_queue*)malloc(sizeof(struct msg_queue));
    TAILQ_INIT(&(pMsgQueue->entries));
    pthread_mutex_unlock(&(pMsgQueue->lock));

    struct event eTimeout;
    struct timeval tTimeout = {2, 0};
    event_assign(&eTimeout, pEventBase, -1, EV_PERSIST, do_read, pMsgQueue);
    evtimer_add(&eTimeout, &tTimeout);

    // 这里其实没有使用event库
    pthread_t tid;
    pthread_create(&tid, NULL, do_write, pMsgQueue);

    event_base_dispatch(pEventBase);
    event_base_free(pEventBase);

    return 0;
}