/*
 * @Description  : HTTP连接类
 * @Date         : 2022-07-16 01:14:05
 * @LastEditTime : 2022-07-16 23:55:47
 */
#ifndef MY_WEBSERVER_HTTPCONN_H
#define MY_WEBSERVER_HTTPCONN_H

#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>

#include "../buffer/buffer.h"
#include "../logsys/log.h"
#include "../pool/sqlconnRAII.h"
#include "httprequest.h"
#include "httpresponse.h"

class HttpConn {
public:
    // 静态
    static bool             isET;       // 指示处理客户端连接的工作模式
    static char            *srcDir;     // 资源文件目录
    static std::atomic<int> userCount;  // 指示用户连接个数

private:
    int         fd_;       // socket对应的文件描述符
    sockaddr_in addr_;     // socket对应的地址
    bool        isClose_;  // 指示这个连接是否关闭

    int   iovCnt_;  // 输出数据的个数，不在连续区域
    iovec iov_[2];  // 代表输出哪些数据的结构体

    Buffer readBuff_;   // 读缓冲区
    Buffer writeBuff_;  // 写缓冲区

    HttpRequest  request_;   // 包装的处理http请求的类
    HttpResponse response_;  // 包装的处理http回应的类

public:
    HttpConn();
    ~HttpConn();

    void init(int sockfd, const sockaddr_in &addr);

    ssize_t read(int *saveErrno);
    ssize_t write(int *saveErrno);

    void closeConn();

    int getFd() const;
    int getPort() const;

    const char *getIP() const;
    sockaddr_in getAddr() const;

    bool process();

    int bytesNeedWrite();

    bool isKeepAlive() const;
};

#endif  //MY_WEBSERVER_HTTPCONN_H
