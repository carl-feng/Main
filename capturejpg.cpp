//包含zmq的头文件 
#include <zmq.h>
#include <stdio.h>
#include <string>
#include <cstring>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

int get_lock(void)
{
    int fdlock;
    struct flock fl;
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 1;
    if((fdlock = open("/tmp/capturejpg.lock", O_WRONLY|O_CREAT, 0666)) == -1)
        return 0;
    if(fcntl(fdlock, F_SETLK, &fl) == -1)
        return 0;
    return 1;
}

int main(int argc, char * argv[])
{
    void * pCtx = NULL;
    void * pSock = NULL;

    while(!get_lock())
    {
        printf("another instance(capturejpg) is running, retry later...\n");
        sleep(1);
    }
    //通信使用的网络端口 为7766 
    const char * pAddr = "tcp://localhost:7766";

    char buf[1024] = { 0 };
    std::string curPath;
    int n = readlink("/proc/self/exe" , buf , sizeof(buf));
    if( n > 0 && n < sizeof(buf))
    {
        curPath = buf;
        curPath = curPath.substr(0, curPath.find_last_of('/'));
        chdir(curPath.c_str());
    }
    
    //创建context 
    if((pCtx = zmq_ctx_new()) == NULL)
    {
        return 0;
    }
    //创建socket 
    if((pSock = zmq_socket(pCtx, ZMQ_DEALER)) == NULL)
    {
        zmq_ctx_destroy(pCtx);
        return 0;
    }
    int iSndTimeout = 5000;// millsecond
    //设置接收超时 
    if(zmq_setsockopt(pSock, ZMQ_RCVTIMEO, &iSndTimeout, sizeof(iSndTimeout)) < 0)
    {
        zmq_close(pSock);
        zmq_ctx_destroy(pCtx);
        return 0;
    }
    //连接目标IP192.168.1.2，端口7766 
    if(zmq_connect(pSock, pAddr) < 0)
    {
        zmq_close(pSock);
        zmq_ctx_destroy(pCtx);
        return 0;
    }
    char szMsg[10] = {0};
    for(int i = 0; i < 5; i++)
    {
        zmq_send(pSock, "YUYV", 4, 0);
        zmq_recv(pSock, szMsg, sizeof(szMsg), 0);
    }
    printf("filename = %s\n", argv[1]);
    if(zmq_send(pSock, argv[1], strlen(argv[1]), 0) < 0)
    {
        fprintf(stderr, "capture faild\n");
        return -1;
    }
    zmq_recv(pSock, szMsg, sizeof(szMsg), 0);
    printf("capture finished\n");
    if(strcmp(szMsg, "OK") == 0) 
        return 0;
    
    return -1;
}
