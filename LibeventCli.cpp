#include "LibeventCli.h"
#include <event2/util.h>
#include <errno.h>
#include <string.h>
#include <sstream>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/select.h>
#include <fcntl.h>

using namespace std;

UnixCli::UnixCli(/* args */)
{
    m_destAddr = "0.0.0.0";
    m_destPort = 8000;
    m_fd = -1;
    m_base = NULL;
    m_buff_event = NULL;
}

UnixCli::UnixCli(const std::string &destAddr,const int destPort)
{
    m_destAddr = destAddr;
    m_destPort = destPort;
    m_fd = -1;
    m_base = NULL;
    m_buff_event = NULL;
}

UnixCli::~UnixCli()
{
    //一定要先释放 bufferevent 再释放 event_base，不然 coredump 找都找不到
    if (m_buff_event)
        bufferevent_free(m_buff_event); //释放消息缓冲套接字
    if (m_base)
        event_base_free(m_base); //销毁libevent
}

void UnixCli::run()
{
    evthread_use_pthreads(); //-levent_pthreads 多线程的时候用
#ifdef UNIX
    int unixTimeout = 1000;
    /*  连接到 msg->getDevKey() 服务器地址所启动的 Tcp 服务 */
    if (-1 == (m_fd = createUnixConnect(MASAGEPROCSOCKPATH, &unixTimeout)))
    {
        cout << "connect unix conn addr:" << MASAGEPROCSOCKPATH << " error!!" << endl;
        return;
        exit(0);
    }
#else
    int unixTimeout = 1000;
    if (-1 == (m_fd = createInetConnect(m_destAddr, m_destPort, &unixTimeout)))
    {
        cout << "connect inet conn addr:" << m_destAddr << ",port:"<<m_destPort<<" error!!" << endl;
        return;
        exit(0);
    }
#endif
    
    int on = 1;
    int keepIdle = 60;
    int keepInterval = 20;
    int keepCount = 3;

    evutil_make_socket_nonblocking(m_fd);
    setsockopt(m_fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));

    setsockopt(m_fd, SOL_SOCKET, TCP_KEEPIDLE, (void *)&keepIdle, sizeof(keepIdle));
    setsockopt(m_fd, SOL_SOCKET, TCP_KEEPINTVL, (void *)&keepInterval, sizeof(keepInterval));
    setsockopt(m_fd, SOL_SOCKET, TCP_KEEPCNT, (void *)&keepCount, sizeof(keepCount));

    m_base = event_base_new();
    /*创建基于套接字的 bufferevent */
    m_buff_event = bufferevent_socket_new(m_base, m_fd, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_enable(m_buff_event, EV_READ | EV_WRITE | EV_PERSIST);
    /*  绑定事件， 接收读事件和错误事件  */
    bufferevent_setcb(m_buff_event, UnixCli::parseReadEvent, NULL, parseErrorEvent, this /*传递的参数*/);

    struct timeval tv = {1000, 0};
    bufferevent_set_timeouts(m_buff_event, &tv, NULL);

    event_base_dispatch(m_base);    /*进入事件循环*/
    // event_base_loop(m_base,EVLOOP_ONCE);    //阻塞等待任意一个事件到来时，结束事假循环
}

void UnixCli::sendReq(IN std::string bsAddr, IN int bsPort, IN void *reqData, IN size_t reqDataLen)
{
    if (m_buff_event == NULL)
        return;
    int len = bufferevent_write(m_buff_event, reqData, reqDataLen);

    if (-1 == len)
    {
        bufferevent_free(m_buff_event);
        return;
    }

}

void UnixCli::parseReadEvent(struct bufferevent *bev, void *arg)
{
    char buf[1024];
    memset(buf, '\0', 1024);
    UnixCli *pThis = (UnixCli *)arg;
    evutil_socket_t fd = bufferevent_getfd(bev);
    int readLen = bufferevent_read(bev, &buf, sizeof(buf));
    cout << "Read fd " << fd << ",dataLen " << readLen << ": " << buf ;
}

void UnixCli::parseErrorEvent(struct bufferevent *bev, short event, void *arg)
{
    UnixCli *pThis = (UnixCli *)arg;
    evutil_socket_t fd = bufferevent_getfd(bev);
    cout << "ErrorEvent fd " << fd << endl;
    bufferevent_free(bev);
    event_base_loopexit(pThis->m_base, NULL);
}

int UnixCli::createUnixConnect(const std::string &destAddr, int *pTimeOut)
{
    int sockfd = -1;
    int ret = -1;
    struct sockaddr_un server_addr;
    int status = -1;
    if ((sockfd = socket(PF_UNIX, SOCK_STREAM, 0)) == -1)
        return -1;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, MASAGEPROCSOCKPATH);

    if (pTimeOut == NULL)
    {
        status = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (status < 0)
            return status;
        return sockfd;
    }

    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);

    if ((ret = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr))) == 0)
    {
        cout << "connect " << destAddr << " success!!" << endl;
        return sockfd;
    }

    if (errno != EINPROGRESS)
    {
        cout << "connect " << destAddr << " failed!!"
             << " errno:" << errno << " " << strerror(errno) << endl;
        close(sockfd);
        return -1;
    }

    struct timeval tv;
    tv.tv_sec = *pTimeOut / 1000;
    tv.tv_usec = (*pTimeOut % 1000) * 1000;
    fd_set fdset;
    FD_ZERO(&fdset);
    /*  将 socket 添加到 select 的监听事件  */
    FD_SET(sockfd, &fdset);
    /*    拥塞等待文件描述符可写事件的到来    */
    if (select(sockfd + 1, NULL, &fdset, NULL, &tv) != 0)
    {
        if (FD_ISSET(sockfd, &fdset))
        {
            int sockopt = -1;
            socklen_t socklen = sizeof(socklen_t);
            if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &sockopt, &socklen) < 0)
            {
                status = -1;
            }
            if (sockopt != 0)
            {
                status = -1;
            }
            status = 0;
        }
    }

    if (status == 0)
    {
        return sockfd;
    }
    else
    {
        close(sockfd);
        return -1;
    }
}
int UnixCli::createInetConnect(const std::string &destAddr, const int destPort, int *pTimeOut)
{
    int sockfd = -1;
    int ret = -1;
    struct sockaddr_in server_addr;
    int status = -1;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        return -1;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(destPort);
    server_addr.sin_addr.s_addr = inet_addr(destAddr.c_str());
    // if(inet_pton(AF_INET , destAddr.c_str() , &server_addr.sin_addr) < 0)
	// {
	// 	printf("inet_pton error for %s\n",destAddr.c_str());
	// 	exit(1);
	// }

    if (pTimeOut == NULL)
    {
        status = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (status < 0)
            return status;
        return sockfd;
    }

    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);

    if ((ret = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr))) == 0)
    {
        cout << "connect " << destAddr << " success!!" << endl;
        return sockfd;
    }

    if (errno != EINPROGRESS)
    {
        cout << "connect addr:" << destAddr << ",port:" << destPort << " failed!!"
             << " errno:" << errno << " " << strerror(errno) << endl;
        close(sockfd);
        return -1;
    }

    struct timeval tv;
    tv.tv_sec = *pTimeOut / 1000;
    tv.tv_usec = (*pTimeOut % 1000) * 1000;
    fd_set fdset;
    FD_ZERO(&fdset);
    /*  将 socket 添加到 select 的监听事件  */
    FD_SET(sockfd, &fdset);
    /*    拥塞等待文件描述符可写事件的到来    */
    if (select(sockfd + 1, NULL, &fdset, NULL, &tv) != 0)
    {
        if (FD_ISSET(sockfd, &fdset))
        {
            int sockopt = -1;
            socklen_t socklen = sizeof(socklen_t);
            if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &sockopt, &socklen) < 0)
            {
                status = -1;
            }
            if (sockopt != 0)
            {
                status = -1;
            }
            status = 0;
        }
    }

    if (status == 0)
    {
        return sockfd;
    }
    else
    {
        close(sockfd);
        return -1;
    }
}