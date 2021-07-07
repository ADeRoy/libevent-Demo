### Libevent

> libevent 是一个轻量级的事件触发的网络库。它适用于windows、linux、bsd等多种平台，它是跨平台的。libevent是c语言编写的一个开源的网络库。

**获取源码**

> Github： https://github.com/libevent/libevent

### libevent安装(ubantu16.04安装)

~~~shell
su root	#必须为root用户下，否则make install失败
wget http://monkey.org/~provos/libevent-1.4.14b-stable.tar.gz
tar xzf libevent-1.4.14b-stable.tar.gz
cd libevent-1.4.14b-stable
./configure --prefix=/opt/libevent
make
make install
~~~

wget失败直接去官网下载Release版本，地址：https://github.com/libevent/libevent/releases

库安装完成之后，**查看库是否安装成功**

~~~shell
➜  Desktop ls -al /opt/libevent
total 24
drwxr-xr-x 6 root root 4096 Dec  4 21:48 .
drwxr-xr-x 4 root root 4096 Dec  4 21:48 ..
drwxr-xr-x 2 root root 4096 Dec  4 21:48 bin
drwxr-xr-x 2 root root 4096 Dec  4 21:48 include
drwxr-xr-x 2 root root 4096 Dec  4 21:48 lib
drwxr-xr-x 3 root root 4096 Dec  4 21:48 share
~~~

**测试示例**

~~~c
#include <stdio.h>  
#include <stdlib.h>  
#include <unistd.h>  
#include <sys/types.h>      
#include <sys/socket.h>      
#include <netinet/in.h>      
#include <arpa/inet.h>     
#include <string.h>  
#include <fcntl.h>   
  
#include <event2/event.h>  
#include <event2/bufferevent.h>  
  
int main() {  
    puts("init a event_base!");  
    struct event_base *base; //定义一个event_base  
    base = event_base_new(); //初始化一个event_base  
    const char *x =  event_base_get_method(base); //查看用了哪个IO多路复用模型，linux一下用epoll  
    printf("METHOD:%s\n", x);  
    int y = event_base_dispatch(base); //事件循环。因为我们这边没有注册事件，所以会直接退出  
    event_base_free(base);  //销毁libevent  
    return 1;  
}
~~~

~~~shell
➜  Desktop gcc -o event_base event_base.c -levent
event_base.c:11:28: fatal error: event2/event.h: No such file or directory
 #include <event2/event.h>  
                            ^
compilation terminated.
~~~

**报错解决方案**：安装`libevent-dev`

~~~shell
sudo apt-get install libevent-dev
[sudo] password for deroy: 
E: Could not get lock /var/lib/dpkg/lock-frontend - open (11: Resource temporarily unavailable)
E: Unable to acquire the dpkg frontend lock (/var/lib/dpkg/lock-frontend), is another process using it?
~~~

**无法解锁xxx**，不用管网上的`kill`所有`进程`，直接`rm /var/lib/dpkg/lock-frontend`

~~~shell
➜  Desktop sudo rm /var/lib/dpkg/lock-frontend 
➜  Desktop sudo apt-get install libevent-dev  
E: Could not get lock /var/lib/dpkg/lock - open (11: Resource temporarily unavailable)
E: Unable to lock the administration directory (/var/lib/dpkg/), is another process using it?
~~~

有报错？继续`rm /var/lib/dpkg/lock`

~~~shell
➜  Desktop sudo apt-get install libevent-dev
Reading package lists... Done
...
...
Processing triggers for libc-bin (2.23-0ubuntu11) ...
~~~

**大功告成**

~~~shell
➜  Desktop gcc -o event_base event_base.c -levent
➜  Desktop ./event_base
init a event_base!
METHOD:epoll
~~~

测试成功：这只是一个简单的demo

### 初识libevent

libevent提供的simple有点劝退，这里用《Linux高性能服务器编程》中Libevent源码分析里面的demo

~~~c
#include <sys/signal.h>
#include <event.h>

void signal_cb( int fd, short event, void* argc )
{
    struct event_base* base =(struct event_base*)argc;
    struct timeval delay = { 2, 0 };
    printf( "Caught an interrupt signal; exiting cleanly in two seconds...\n" );
    event_base_loopexit( base, &delay );
}  

void timeout_cb( int fd, short event, void* argc )
{
    printf( "timeout\n" );
}

int main()  
{  
    struct event_base* base = event_init();

    struct event* signal_event = evsignal_new( base, SIGINT, signal_cb, base );
    event_add( signal_event, NULL );

    struct timeval tv = { 1, 0 };
    struct event* timeout_event = evtimer_new( base, timeout_cb, NULL );
    event_add( timeout_event, &tv );

    event_base_dispatch( base );

    event_free( timeout_event );
    event_free( signal_event );
    event_base_free( base );
}  
~~~

**编译运行**

~~~shell
gcc -o libevent_test libevent_test.c -levent
./libevent_test
timeout
^CCaught an interrupt signal; exiting cleanly in two seconds...
~~~

Demo简单，但是却描述了LibEvent库的主要逻辑

### libevent开发流程

#### 一、创建event_base对象

调用`event_init`函数创建`event_base`对象。一个`event_base`相当于一个`rector`实例.

~~~c
struct event_base* base = event_init();
~~~

#### 二、创建具体的事件处理器

创建具体的事件处理器，并设置从属的Rector实例,`evsignal_new`和`evtimer_new`分别用于创建**信号事件处理器**和**定时事件处理器**。

~~~c
struct event* signal_event = evsignal_new( base, SIGINT, signal_cb, base );//创建信号事件处理器并设置从属的Rector实例
struct event* timeout_event = evtimer_new( base, timeout_cb, NULL );//创建定时事件处理器并设置从属的Rector实例
~~~

这两个函数有一个统一入口函数`event_new`

~~~c
struct event* event_new(struct event_base* base，evutil_socket_t  fd，short what，event_callback_fn  cb，void* arg);
~~~

参数1：base 指定新创建的事件处理器从属的 Rector，

参数2：fd指定与该事件处理器关联的句柄

参数3：events需要监控的事件

~~~c
EV_TIMEOUT      0x01	定时事件
EV_READ			0x02	可读事件
EV_WRITE		0x04	可写事件
EV_SIGNAL		0x08	信号事件
EV_PERSIST		0x10	永久事件
/*边沿触发事件，需要I/O复用系统调用支持*/
EV_ET			0x20
~~~

参数4：callback指定目标事件的回调函数

参数5：callback_arg

参数6：传递给回调函数的参数

函数返回值：成功返回一个event类型对象,

#### 三、将事件处理器添加到注册事件队列

调用`event_add`函数将事件处理器添加到注册事件队列中去

#### 四、执行事件循环

调用`event_base_dispatch`函数执行事件循环

#### 五、释放资源

事件循环之后free掉资源

~~~c
if (m_buff_event)
{
    bufferevent_free(m_buff_event); //释放消息缓冲套接字
    m_buff_event = NULL;
}
if (m_base)
{
    event_base_free(m_base); //销毁libevent
    m_base = NULL;
}
~~~

客户端一定要先释放 `bufferevent`  再释放 `event_base`

### libevent里面的一些函数

**设置端口重用**  

~~~c
evutil_make_listen_socket_reuseable(server_socketfd); 
~~~

**设置无阻赛**  实体在evutil.c中，是对fcntl操作

~~~c
evutil_make_socket_nonblocking(server_socketfd); 
~~~

返回一个字符串，**标识内核事件机制**（kqueue的，epoll的，等等）

~~~c
const char *x =  event_base_get_method(base); //查看用了哪个IO多路复用模型，linux一下用epoll 
~~~

程序**进入无限循环**，等待就绪事件并执行事件处理

~~~c
int event_base_dispatch(struct event_base *);
~~~

| 函数声明                                                     | 功能                                                         |
| ------------------------------------------------------------ | ------------------------------------------------------------ |
| const char **event_get_supported_methods(void);              | 返回一个指针 ,指向 libevent 支持的IO多路方法名字数组，这个数组的最后一个元素是NULL |
| const char *event_base_get_method(const struct event_base *base); | 返回 event_base 正在使用的IO多路方法                         |
| enum event_method_feature event_base_get_features(const struct event_base *base); | 返回 event_base 支持的特征的比特掩码                         |

**属性获取示例**

~~~c
#include <event2/event.h>  
#include <stdio.h>  

int main()
{
	//libevent的版本
	printf("Starting Libevent %s. Available methods are:\n", event_get_version());

	//检查支持的IO多路方法
	const char **methods = event_get_supported_methods();
	for (int i=0; methods[i] != NULL; ++i) {
		printf(" %s\n", methods[i]);
	}
	
	struct event_base *base = event_base_new();
	enum event_method_feature f;

	if (!base) 
	{
		puts("Couldn't get an event_base!");
	} 
	else 
	{
		//返回 event_base 正在使用的IO多路方法
		printf("Using Libevent with backend method :%s\n",event_base_get_method(base));
	
		//返回 event_base 支持的特征的比特掩码
		f = event_base_get_features(base);
		if ((f & EV_FEATURE_ET)) //支持边沿触发的后端
			printf(" Edge-triggered events are supported.\n");
		if ((f & EV_FEATURE_O1)) //添加、删除单个事件，或者确定哪个事件激活的操作是 O（1）复杂度的后端
			printf(" O(1) event notification is supported.\n");
		if ((f & EV_FEATURE_FDS)) //要求支持任意文件描述符，而不仅仅是套接字的后端
			printf(" All FD types are supported.\n");
	}
}
~~~

~~~shell
Starting Libevent 2.0.21-stable. Available methods are:
 epoll
 poll
 select
Using Libevent with backend method :epoll
 Edge-triggered events are supported.
 O(1) event notification is supported.

~~~

### 事件循环 event_loop

> 一旦创建好事件根基`event_base`，并且在根基上安插好事件之后，需要对事件循环监控(换句话说就是等待事件的到来，触发事件的回调函数)，有两种方式可以达到上面描述的功能，即：`event_base_dispatch`和`event_base_loop`

~~~c
int event_base_dispatch(struct event_base *);	//程序进入无限循环，等待就绪事件并执行事件处理
int event_base_loop(struct event_base *base, int flags);	//
~~~

**参数flags**

- **EVLOOP_ONCE**：相当于epoll_wait阻塞方式&&只调用一次 ⇒ 当没有事件到来时，程序将一直阻塞在event_base_loop函数；直到有任意一个事件到来时，程序才不会阻塞在event_base_loop，将会继续向下执行。
- **EVLOOP_NONBLOCK**：相当于epoll_wait非阻塞方式&&只调用一次 ⇒ 即使没有事件到来，程序也不会阻塞在event_base_loop
- **EVLOOP_NO_EXIT_ON_EMPTY**：等价于event_base_dispatch ⇒ 将一直循环监控事件 ⇒ 直到没有已经注册的事件 || 调用了event_base_loopbreak()或 event_base_loopexit()为止

#### 事件循环的退出的情况

引起循环退出的情况：

+ event_base中没有事件了 
+ 调用event_base_loopbreak 事件循环会停止 (立即停止)
+ 调用event_base_loopexit  (等待所有事件结束后停止)
+ 程序错误 

### libevent简易聊天室

~~~c
#include <stdio.h>  
#include <stdlib.h>  
#include <unistd.h>  
#include <sys/types.h>      
#include <sys/socket.h>      
#include <netinet/in.h>      
#include <arpa/inet.h>     
#include <string.h>  
#include <fcntl.h>   
  
#include <event2/event.h>  
#include <event2/bufferevent.h> 

int cli_socket[1024]; 
int cli_index = 0;
  
//读取客户端  
void do_read(evutil_socket_t fd, short event, void *arg) {  
    //继续等待接收数据    
    char buf[1024];  //数据传送的缓冲区      
    int len;    
    if ((len = recv(fd, buf, 1024, 0)) > 0)  {    
        buf[len] = '\0';      
        printf("%s", buf); 
		for(int index = 0;index<cli_index;index++){
			if (send(cli_socket[index], buf, len, 0) < 0) {    //将接受到的数据写回客户端  
				perror("write");      
			}
		}
    }
	if(len == 0)
	{
		printf("fd:%d close\n",fd);
		close(fd);
	}
	if(len <0)
	{
		perror("recv");     
	}
}   
  
//回调函数，用于监听连接进来的客户端socket  
void do_accept(evutil_socket_t fd, short event, void *arg) {  
    int client_socketfd;//客户端套接字      
    struct sockaddr_in client_addr; //客户端网络地址结构体     
    int in_size = sizeof(struct sockaddr_in);    
    //客户端socket    
    client_socketfd = accept(fd, (struct sockaddr *) &client_addr, &in_size); //等待接受请求，这边是阻塞式的    
    if (client_socketfd < 0) {    
        puts("accpet error");    
        exit(1);  
    } 
	cli_socket[cli_index++] = client_socketfd;
	printf("Connect from %s:%u ...!\n",inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port)); 
    //类型转换
    struct event_base *base_ev = (struct event_base *) arg;  
  
    //socket发送欢迎信息    
    char * msg = "Welcome to Libevent socket\n";    
    int size = send(client_socketfd, msg, strlen(msg), 0);    
  
    //创建一个事件，这个事件主要用于监听和读取客户端传递过来的数据  
    //持久类型，并且将base_ev传递到do_read回调函数中去  
    struct event *ev;  
    ev = event_new(base_ev, client_socketfd, EV_TIMEOUT|EV_READ|EV_PERSIST, do_read, base_ev);  
    event_add(ev, NULL);  
}  
  
  
//入口主函数  
int main() {  
  
    int server_socketfd; //服务端socket    
    struct sockaddr_in server_addr;   //服务器网络地址结构体      
    memset(&server_addr,0,sizeof(server_addr)); //数据初始化--清零      
    server_addr.sin_family = AF_INET; //设置为IP通信      
    server_addr.sin_addr.s_addr = INADDR_ANY;//服务器IP地址--允许连接到所有本地地址上      
    server_addr.sin_port = htons(8001); //服务器端口号      
    
    //创建服务端套接字    
    server_socketfd = socket(PF_INET,SOCK_STREAM,0);    
    if (server_socketfd < 0) {    
        puts("socket error");    
        return 0;    
    }    
  
    evutil_make_listen_socket_reuseable(server_socketfd); //设置端口重用  
    evutil_make_socket_nonblocking(server_socketfd); //设置无阻赛  
    
    //绑定IP    
    if (bind(server_socketfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr))<0) {    
        puts("bind error");    
        return 0;    
    }    
  
    //监听,监听队列长度 5    
    listen(server_socketfd, 10);    
    printf("listen port:%d\n",8001);
    //创建event_base 事件的集合，多线程的话 每个线程都要初始化一个event_base  
    struct event_base *base_ev;  
    base_ev = event_base_new();   
    const char *x =  event_base_get_method(base_ev); //获取IO多路复用的模型，linux一般为epoll  
    printf("METHOD:%s\n", x);  
  
    //创建一个事件，类型为持久性EV_PERSIST，回调函数为do_accept（主要用于监听连接进来的客户端）  
    //将base_ev传递到do_accept中的arg参数  
    struct event *ev;  
    ev = event_new(base_ev, server_socketfd, EV_TIMEOUT|EV_READ|EV_PERSIST, do_accept, base_ev);  
  
    //注册事件，使事件处于 pending的等待状态  
    event_add(ev, NULL);  
  
    //事件循环  
    event_base_dispatch(base_ev);  
  
    //销毁event_base  
    event_base_free(base_ev);    
    return 1;  
}
~~~

~~~shell
gcc -o socket socket.c -levent
./socket
listen port:8001
METHOD:epoll
~~~

~~~shell
nc 0.0.0.0 8001
~~~

### 企业级开发

#### 服务端Svr

#### LibeventSvr.h

~~~c
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
~~~

#### LibeventSvr.cpp

~~~c
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
~~~

#### 测试服务端

main.cpp

~~~c
#include "LibeventSvr.h"
int main()
{
    MasageSvr svr;
    svr.Start("0.0.0.0", 8000);
    return 0;
}
~~~

**编译**

~~~shell
g++ -o svr main.cpp LibeventSvr.cpp -levent -lpthread -levent_pthreads
~~~

运行结果：

~~~shell
root@VM-16-5-ubuntu:/home/ubuntu/libevent-demo# ./svr 
start. listen ip:, port:8000
~~~

nc模拟客户端连接

~~~shell
root@VM-16-5-ubuntu:/home/ubuntu/libevent-demo# nc 0.0.0.0 8000
hello
hello
^C
~~~

服务端接收连接，读取客户端发过来的数据，将数据回射给客户端，客户端退出

~~~shell
root@VM-16-5-ubuntu:/home/ubuntu/libevent-demo# ./svr 
start. listen ip:, port:8000
accept client ip:127.0.0.1, port: 53960 ,fd: 8
Read fd 8,dataLen 6: hello
client(,8000,) bufferevent [reading error][connection closed], event: 17
~~~

#### 客户端Cli（UNIX）

测试用例使用的是 `UNIX` 本地套接字编程，主要用于消息的传递。客户端在发送完请求后等待响应，接收到响应后进行相关工作，然后关闭通信。

使用客户端案例(`UNIX`)之前打开 `LibeventSvr.h` 里面的 `UNIX` 定义

#### LibeventCli.h

~~~c
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
~~~

#### LibeventCli.cpp

~~~c
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
~~~

#### 测试客户端

**client.cpp**

~~~c
#include "LibeventCli.h"
#include <pthread.h>
#include <unistd.h>
#include <string.h>

void *thread_run(void *arg)
{
    UnixCli *cli = (UnixCli *)arg;
    char sendBuf[1024];
    while(1)
    {
        memset(sendBuf,'\0',sizeof(sendBuf));
        read(0,sendBuf,sizeof(sendBuf));
        cli->sendReq("0.0.0.0",8000,(void*)sendBuf,sizeof(sendBuf));
    }
}
int main()
{
    UnixCli* cli = new UnixCli;
    pthread_t cliThread;
    //新建子进程单独处理链接
    pthread_create(&cliThread, NULL, thread_run, (void*)cli);
    pthread_detach(cliThread);
    cli->run();
    return 0;
}
~~~

#### 编译makefile

~~~makefile
DEBUF = #-DUNIX
all:server client chat
server:server.cpp
	g++ -o svrver server.cpp LibeventSvr.cpp -levent -lpthread -levent_pthreads $(DEBUF)
client:client.cpp
	g++ -o client client.cpp LibeventCli.cpp -levent -lpthread -levent_pthreads $(DEBUF)
chat:chatRoom.c
	gcc -o chat chatRoom.c -levent -lpthread 
.PHONY:clean
clean:
	rm -f svrver client chat test.sock
~~~

#### 运行结果

开启 `EVENT` 宏

Server

~~~shell
root@VM-16-5-ubuntu:/home/ubuntu/libevent-demo/libevent# ./svrver 
start . listen UNIX:test.sock
accept client addr:test.sock
Read fd 8,dataLen 1024: hello
~~~

Client

~~~shell
root@VM-16-5-ubuntu:/home/ubuntu/libevent-demo/libevent# ./client 
connect test.sock success!!
hello
Read fd 3,dataLen 1024: hello
~~~

关闭 `EVENT` 宏

 Server

~~~shell
root@VM-16-5-ubuntu:/home/ubuntu/libevent-demo/libevent# ./svrver 
start. listen ip:0.0.0.0, port:8000
accept client ip:127.0.0.1, port: 32964 ,fd: 8
Read fd 8,dataLen 1024: hello
~~~

Client

~~~shell
root@VM-16-5-ubuntu:/home/ubuntu/libevent-demo/libevent# ./client 
hello
Read fd 3,dataLen 1024: hello
~~~

