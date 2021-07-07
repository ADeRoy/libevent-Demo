#ifndef _LIBEVENTSVR_H_
#define _LIBEVENTSVR_H_

#include <iostream>

#ifdef __cplusplus
extern "C"
{
#endif

#include "event2/listener.h"
#include "event2/thread.h"
#include "event2/bufferevent.h"
#include "event2/event_compat.h"
#include "event2/buffer.h"

#include <event2/event.h>
#include <event.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#ifdef __cplusplus
}
#endif

/**
     * 函数参数标识宏
     */
#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

#ifndef INOUT
#define INOUT
#endif

//是否启用UNIX本地套接字进行通信
// #ifndef UNIX
// #define UNIX
// #endif

//本地套接字地址
#define MASAGEPROCSOCKPATH "test.sock"

class MasageSvr
{
private:
    /* data */
    struct event_base *m_base;

    int m_backLogCnt;

    int m_listenPort;
    std::string m_listenAddr;

public:
    MasageSvr(/* args */);
    ~MasageSvr();
    void Start(IN std::string bsAddr, IN int bsPort);

public:
    static void onAcceptErrorCb(struct evconnlistener *listener, void *arg);
    static void onAcceptConnCb(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen, void *arg);
    static void onClientReadCb(struct bufferevent *bev, void *arg);
    static void onClientWrtCb(struct bufferevent *bev, void *arg);
    static void onClientEvtCb(struct bufferevent *bev, short event, void *arg);

public:
    int handleEvent(short event);
};

#endif