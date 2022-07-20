/*
 * @Description  : 服务器类
 * @Date         : 2022-07-16 01:14:06
 * @LastEditTime : 2022-07-21 02:06:39
 */
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <unordered_map>

#include "../http/httpconn.h"
#include "../json/Json.h"
#include "../logsys/log.h"
#include "../pool/sqlconnRAII.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"
#include "../timer/heaptimer.h"
#include "epoller.h"

using namespace lightJson;

class WebServer {
private:
    /* 构造函数参数 */

    int  port_;        // 监听的端口
    int  trigMode_;    // 触发模式
    int  timeoutMS_;   // 超时时间
    bool openLinger_;  // 优雅关闭
    int  threadNum_;   // 线程数量

    int         sqlPort_;     // 数据库端口
    int         sqlConnNum_;  // MySQL连接数量
    std::string sqlUser_;     // 用户
    std::string sqlPwd_;      // 密码
    std::string dbName_;      // 数据库名称

    bool openLog_;     // 开启日志
    int  logLevel_;    // 日志级别
    int  logQueSize_;  // 日志队列大小

private:
    static const int MAX_FD = 65536;  // 最大文件描述符数量

    static bool isET;             // 指示本地监听的工作模式
    int         listenFd_;        // 监听的文件描述符
    bool        isClose_{false};  // 指示InitSocket操作是否成功
    char       *srcDir_;          // 资源文件目录

    uint32_t listenEvent_;  // 监听描述符上的epoll事件
    uint32_t connEvent_;    // 客户端连接的socket描述符上的epoll事件

    std::unique_ptr<Epoller>          epoller_;     // epoller变量
    std::unique_ptr<HeapTimer>        timer_;       // 基于小根堆的定时器
    std::unique_ptr<ThreadPool>       threadPool_;  // 线程池
    std::unordered_map<int, HttpConn> users_;       // 连接用到时再实例化

public:
    WebServer(const Json &json);

    WebServer(std::tuple<int, int, int, bool, int>                        &webConf,
              std::tuple<int, std::string, std::string, std::string, int> &sqlConf,
              std::tuple<bool, int, int>                                  &logConf);

    ~WebServer();

    void initServer();

    void runServer();

private:
    static int setFdNonblock(int fd);

    bool initListenFd_();
    void initEventMode_();
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

#endif  //WEBSERVER_H
