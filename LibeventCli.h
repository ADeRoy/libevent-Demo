#ifndef _LIBEVENTCLI_H_
#define _LIBEVENTCLI_H_

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

// #ifndef UNIX
// #define UNIX
// #endif

//本地套接字地址
#define MASAGEPROCSOCKPATH "test.sock"

class UnixCli
{
private:
    /* data */
    struct event_base *m_base;
    struct bufferevent *m_buff_event;
    int 		m_destPort;
	std::string m_destAddr;

	int m_fd;

public:
    UnixCli(/* args */);
    UnixCli(const std::string &destAddr,const int destPort);
    ~UnixCli();
    void run();
    void sendReq(IN std::string bsAddr, IN int bsPort, IN void *reqData, IN size_t reqDataLen);
    int  createUnixConnect(const std::string &destAddr, int *pTimeOut);
    int  createInetConnect(const std::string &destAddr, const int port, int *pTimeOut);

public:
    static void parseReadEvent(struct bufferevent* bev, void* arg);
    static void parseErrorEvent(struct bufferevent *bev, short event, void *arg);
};

#endif