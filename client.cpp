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