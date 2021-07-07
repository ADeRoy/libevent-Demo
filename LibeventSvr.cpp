#include "LibeventSvr.h"
#include <event2/util.h>
#include <errno.h>
#include <string.h>
#include <sstream>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;
MasageSvr::MasageSvr(/* args */)
{
    m_listenAddr = "";
    m_listenPort = 8000;
    m_base = NULL;
}

MasageSvr::~MasageSvr()
{
    if (m_base)
        event_base_free(m_base); //销毁libevent
}

void MasageSvr::Start(IN std::string bsAddr, IN int bsPort)
{
    int ret = 0;

    struct evconnlistener *m_listener = NULL;
#ifdef UNIX
    struct sockaddr_un svraddr;
    svraddr.sun_family = AF_UNIX;
    strncpy(svraddr.sun_path, MASAGEPROCSOCKPATH, sizeof(MASAGEPROCSOCKPATH));

    struct stat stbuf;
    if (stat(MASAGEPROCSOCKPATH, &stbuf) != -1)
    {
        // 如果此文件已存在,且是一个SOCKET文件,则删除
        if (S_ISSOCK(stbuf.st_mode))
            unlink(MASAGEPROCSOCKPATH);
    }
#else
    struct sockaddr_in svraddr;
    svraddr.sin_family = AF_INET;
    svraddr.sin_port = htons(m_listenPort);
    svraddr.sin_addr.s_addr = m_listenAddr.empty() ? htonl(INADDR_ANY) : inet_addr(m_listenAddr.c_str());
    m_listenAddr = "0.0.0.0";
#endif

    ret = evthread_use_pthreads(); //-levent_pthreads 少了会 coredump

    m_base = event_base_new();
    m_listener = evconnlistener_new_bind(m_base, onAcceptConnCb, this /*携带的参数 arg*/, LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE | LEV_OPT_THREADSAFE, m_backLogCnt, (struct sockaddr *)&svraddr, sizeof(struct sockaddr_in));

    if (!m_listener)
    {
#ifdef UNIX
        cout << "connect " << MASAGEPROCSOCKPATH << " error" << endl;
#else
        cout << "connect. listen ip:" << m_listenAddr << ", port:" << m_listenPort << " error" << endl;
#endif

        return;
    }
#ifdef UNIX
    cout << "start . listen UNIX:" << MASAGEPROCSOCKPATH << endl;
#else
    cout << "start. listen ip:" << m_listenAddr << ", port:" << m_listenPort << endl;
#endif
    evconnlistener_set_error_cb(m_listener, onAcceptErrorCb);
    /*  进入事件循环  */
    event_base_dispatch(m_base);
}

/**
 * @fn	onAcceptErrorCb
 * @brief 连接错误回调
 * @param[in]
 * @param[out]
 * @return
 * @retval
 */
void MasageSvr::onAcceptErrorCb(struct evconnlistener *listener, void *arg)
{
    struct event_base *base = evconnlistener_get_base(listener);

    int err = EVUTIL_SOCKET_ERROR();

    cout << "Got an error on the listener: " << evutil_socket_error_to_string(err);

    event_base_loopexit(base, NULL);
    return;
}

/**
 * @fn	onAcceptConnCb
 * @brief 连接监听回调
 * @param[in]
 * @param[out]
 * @return
 * @retval
 */
void MasageSvr::onAcceptConnCb(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen, void *arg)
{
#ifdef UNIX
    cout << "accept client addr:" << MASAGEPROCSOCKPATH << endl;
#else
    cout << "accept client ip:" << inet_ntoa(((struct sockaddr_in *)address)->sin_addr) << ", port: " << ntohs(((struct sockaddr_in *)address)->sin_port) << " ,fd: " << fd << endl;
#endif

    MasageSvr *pThis = (MasageSvr *)arg;
    evutil_make_socket_nonblocking(fd);
    int on = 1;
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
    int keepIdle = 60;
    int keepInterval = 20;
    int keepCount = 3;

    setsockopt(fd, SOL_SOCKET, TCP_KEEPIDLE, (void *)&keepIdle, sizeof(keepIdle));
    setsockopt(fd, SOL_SOCKET, TCP_KEEPINTVL, (void *)&keepInterval, sizeof(keepInterval));
    setsockopt(fd, SOL_SOCKET, TCP_KEEPCNT, (void *)&keepCount, sizeof(keepCount));

    /*  这个 new_buff_event 就是消息缓冲套接字 作用：用于和客户端交互（读写数据） */
    struct bufferevent *new_buff_event = bufferevent_socket_new(pThis->m_base, fd, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);
    bufferevent_setcb(new_buff_event, MasageSvr::onClientReadCb, MasageSvr::onClientWrtCb, MasageSvr::onClientEvtCb, (void *)arg);
    bufferevent_enable(new_buff_event, EV_READ | EV_WRITE | EV_PERSIST);
    return;
}

/**
 * @fn	onClientReadCb
 * @brief 数据输入回调
 * @param[bev]  消息缓冲套接字
 * @param[arg]  回调传递的参数
 * @return
 * @retval
 */
void MasageSvr::onClientReadCb(struct bufferevent *bev, void *arg)
{
    char buf[1024];
    memset(buf, '\0', 1024);
    MasageSvr *pThis = (MasageSvr *)arg;
    evutil_socket_t fd = bufferevent_getfd(bev);

    int readLen = bufferevent_read(bev, &buf, sizeof(buf));
    cout << "Read fd " << fd << ",dataLen " << readLen << ": " << buf;
    bufferevent_write(bev, &buf, sizeof(buf));
    return;
}

/**
 * @fn	onClientWrtCb
 * @brief 数据输出回调
 * @param[in]
 * @param[out]
 * @return
 * @retval
 */
void MasageSvr::onClientWrtCb(struct bufferevent *bev, void *arg)
{
    MasageSvr *pThis = (MasageSvr *)arg;
    evutil_socket_t fd = bufferevent_getfd(bev);
    /*获取和客户端交互的缓冲套接字*/
    struct bufferevent *cli_bev = (struct bufferevent *)arg;
    return;
}

/**
 * @fn	onClientEvtCb
 * @brief 数据事件回调
 * @param[in]
 * @param[out]
 * @return
 * @retval
 */
void MasageSvr::onClientEvtCb(struct bufferevent *bev, short event, void *arg)
{
    MasageSvr *pThis = (MasageSvr *)arg;
    evutil_socket_t fd = bufferevent_getfd(bev);
    pThis->handleEvent(event);
    bufferevent_free(bev); //客户端的 bufferevent 在这里释放
    return;
}

int MasageSvr::handleEvent(short event)
{
    int ret = 0;
    std::ostringstream oss;
    std::ostringstream ossEvtKey;

    oss << "client("
        << m_listenAddr
        << ","
        << m_listenPort
        << ","
        << ") bufferevent ";

    if (event & BEV_EVENT_READING)
        ossEvtKey << "[reading error]";
    if (event & BEV_EVENT_WRITING)
        ossEvtKey << "[writing error]";
    if (event & BEV_EVENT_EOF)
        ossEvtKey << "[connection closed]";
    if (event & BEV_EVENT_ERROR)
        ossEvtKey << "[bufferevent error: " << evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()) << "]";
    if (event & BEV_EVENT_TIMEOUT)
        ossEvtKey << "[bufferevent timeout]";

    oss << ossEvtKey.str() << ", event: " << event;
    cout << oss.str() << endl;
}