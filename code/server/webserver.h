/*
 * @Description  : 服务器类
 * @Date         : 2022-07-16 01:14:06
 * @LastEditTime : 2022-07-17 01:58:47
 */
#ifndef MY_WEBSERVER_WEBSERVER_H
#define MY_WEBSERVER_WEBSERVER_H

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <unordered_map>

#include "../http/httpconn.h"
#include "../logsys/log.h"
#include "../pool/sqlconnRAII.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"
#include "../timer/heaptimer.h"
#include "epoller.h"

class WebServer {
private:
    static const int MAX_FD = 65536;  // 最大文件描述符数量

    static bool isET;  // 指示本地监听的工作模式

    int port_;       // 监听的端口
    int listenFd_;   // 监听的文件描述符
    int timeoutMS_;  // 超时时间

    bool openLinger_;  // 对于残存在套接字发送队列中的数据：丢弃或者将发送至对端，优雅关闭连接。
    bool isClose_;     // 指示InitSocket操作是否成功

    char *srcDir_;  // 资源文件目录

    uint32_t listenEvent_;  // 监听描述符上的epoll事件
    uint32_t connEvent_;    // 客户端连接的socket描述符上的epoll事件

    std::unique_ptr<Epoller>          epoller_;     // epoller变量
    std::unique_ptr<HeapTimer>        timer_;       // 基于小根堆的定时器
    std::unique_ptr<ThreadPool>       threadPool_;  // 线程池
    std::unordered_map<int, HttpConn> users_;       // 连接用到时再实例化

public:
    WebServer(int port, int trigMode, int timeoutMS, bool optLinger, int sqlPort,
              const char *sqlUser, const char *sqlPwd, const char *dbName, int connPoolNum,
              int threadNum, bool openLog, int logLevel, int logQueSize);

    ~WebServer();

    void start();

private:
    static int setFdNonblock(int fd);

    bool initSocket_();
    void initEventMode_(int trigMode);
    void addClient_(int fd, sockaddr_in addr);

    void dealListen_();
    void dealWrite_(HttpConn *client);
    void dealRead_(HttpConn *client);

    void sendError_(int fd, const char *info);
    void extentTime_(HttpConn *client);

    void closeConn_(HttpConn *client);

    void onRead_(HttpConn *client);
    void onWrite_(HttpConn *client);
    void onProcess_(HttpConn *client);
};

#endif  //MY_WEBSERVER_WEBSERVER_H
