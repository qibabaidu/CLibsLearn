#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
 
#include <event2/event.h>
#include <event2/bufferevent.h>
 
#define LISTEN_PORT 9999
#define LISTEN_BACKLOG 32
#define MAX_LINE    256
 
void do_accept(evutil_socket_t listener, short event, void *arg);
void read_cb(struct bufferevent *bev, void *arg);
void error_cb(struct bufferevent *bev, short event, void *arg);
void write_cb(struct bufferevent *bev, void *arg);
 
int main()
{
    //int ret;
    evutil_socket_t listener;//用于跨平台表示socket的ID（在Linux下表示的是其文件描述符）
    listener = socket(AF_INET, SOCK_STREAM, 0);
    assert(listener > 0);
    //用于跨平台将socket设置为可重用（实际上是将端口设为可重用
    evutil_make_listen_socket_reuseable(listener);
 
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = inet_addr("127.0.0.1");
    sin.sin_port = htons(LISTEN_PORT);
 
    if (bind(listener, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("bind");
        return 1;
    }
 
    if (listen(listener, LISTEN_BACKLOG) < 0) {
        perror("listen");
        return 1;
    }
 
    printf ("Listening...\n");
    /* 用于跨平台将socket设置为非阻塞,使用bufferevent需要 */
    evutil_make_socket_nonblocking(listener);
    //主要记录事件的相关属性
    struct event_base *base = event_base_new();
    assert(base != NULL);
    /* Register listen event. */
    struct event *listen_event;
    listen_event = event_new(base, listener, EV_READ|EV_PERSIST, do_accept, (void*)base);
    event_add(listen_event, NULL);
    /* Start the event loop. */
    event_base_dispatch(base);
 
    printf("The End.");
    //close(listener);
    return 0;
}
 
void do_accept(evutil_socket_t listener, short event, void *arg)
{
    struct event_base *base = (struct event_base *)arg;
    evutil_socket_t fd;
    struct sockaddr_in sin;
    socklen_t slen = sizeof(sin);
    fd = accept(listener, (struct sockaddr *)&sin, &slen);
    if (fd < 0) {
        perror("accept");
        return;
    }
    if (fd > FD_SETSIZE) { //这个if是参考了那个ROT13的例子，貌似是官方的疏漏，从select-based例子里抄过来忘了改
        perror("fd > FD_SETSIZE\n");
        return;
    }
 
    printf("ACCEPT: fd = %u\n", fd);
    //关联该sockfd，托管给event_base.
    struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    //设置读写对应的回调函数
    bufferevent_setcb(bev, read_cb, NULL, error_cb, arg);
    //启用读写事件,其实是调用了event_add将相应读写事件加入事件监听队列poll.
    //如果相应事件不置为true，bufferevent是不会读写数据的
    bufferevent_enable(bev, EV_READ|EV_PERSIST);
//    进入bufferevent_setcb回调函数：
//    在readcb里面从input中读取数据，处理完毕后填充到output中；
//    writecb对于服务端程序，只需要readcb就可以了，可以置为NULL；
//    errorcb用于处理一些错误信息
}
 
void read_cb(struct bufferevent *bev, void *arg)
{
    char line[MAX_LINE+1];
    int n;
    evutil_socket_t fd = bufferevent_getfd(bev);
 
    while (n = bufferevent_read(bev, line, MAX_LINE), n > 0)
    {
        line[n] = '\0';
        printf("fd=%u, Server gets the message from client read line: %s\n", fd, line);
	//直接将读取的结果,不做任何修改(本文是跳过前两个字符),直接返回给客户端
	bufferevent_write(bev, line, n);//方案1
    }
}
 
void write_cb(struct bufferevent *bev, void *arg)
{
 printf("HelloWorld\n");//直接空代码即可，因为这里并不会被触发调用
}
 
void error_cb(struct bufferevent *bev, short event, void *arg)
{
    evutil_socket_t fd = bufferevent_getfd(bev);
    printf("fd = %u, ", fd);
    if (event & BEV_EVENT_TIMEOUT) {
        printf("Timed out\n"); //if bufferevent_set_timeouts() called
    }
    else if (event & BEV_EVENT_EOF) {
        printf("connection closed\n");
    }
    else if (event & BEV_EVENT_ERROR) {
        printf("some other error\n");
    }
    bufferevent_free(bev);
}
