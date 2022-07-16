#include "httpconn.h"

char *HttpConn::srcDir = nullptr;
bool  HttpConn::isET   = false;

std::atomic<int> HttpConn::userCount;

HttpConn::HttpConn() : fd_(-1), isClose_(true) { addr_ = {0}; }

/**
 * @description: 初始化httpconn类实例
 * @param {int} sockfd
 * @param {sockaddr_in} &addr
 */
void HttpConn::init(int sockfd, const sockaddr_in &addr) {
    assert(sockfd > 0);
    ++userCount;
    addr_    = addr;
    fd_      = sockfd;
    isClose_ = false;

    writeBuff_.clearAll();
    readBuff_.clearAll();

    LOG_INFO("Client[%d](%s:%d) in, userCount: %d", sockfd, getIP(), getPort(), (int)userCount);
}

HttpConn::~HttpConn() { closeConn(); }

/**
 * @description: 关闭该socket的连接，取消文件映射，关闭文件描述符
 * 被析构函数调用
 */
void HttpConn::closeConn() {
    response_.unmapFile();
    if (!isClose_) {
        isClose_ = true;
        --userCount;
        close(fd_);  // 关闭文件描述符
        LOG_INFO("Client[%d](%s:%d) quit, userCount: %d", fd_, getIP(), getPort(), (int)userCount)
    }
}

bool HttpConn::isKeepAlive() const { return request_.isKeepAlive(); }

int HttpConn::getFd() const { return fd_; }

sockaddr_in HttpConn::getAddr() const { return addr_; }

const char *HttpConn::getIP() const { return inet_ntoa(addr_.sin_addr); }

int HttpConn::getPort() const { return addr_.sin_port; }

/**
 * @description: 读取socket中的数据，保存到读缓冲区中，并设置可能的错误号
 * @param {int} *saveErrno
 */
ssize_t HttpConn::read(int *saveErrno) {
    ssize_t len = -1;
    /*如果是LT模式，那么只读取一次，如果是ET模式，会一直读取，直到读不出数据*/
    do {
        len = readBuff_.readFd(fd_, saveErrno);
        if (len <= 0) {
            break;
        }
    } while (isET);

    return len;
}

/**
 * @description: 返回还需要写多少字节的数据
 */
int HttpConn::bytesNeedWrite() { return iov_[0].iov_len + iov_[1].iov_len; }

/**
 * @description: 使用聚集写writev方法将数据发送到指定socket中，并设置可能的错误号
 * @param {int} *saveErrno
 * @return {*}
 */
ssize_t HttpConn::write(int *saveErrno) {
    ssize_t len = -1;
    do {
        len = writev(fd_, iov_, iovCnt_);
        if (len <= 0) {
            *saveErrno = errno;
            /*若errno返回EAGAIN，需要重新注册EPOLL上的EPOLLOUT事件*/
            break;
        }

        if (bytesNeedWrite() == 0) {
            /* 每次聚集写后都判断是否还有数据需要发送，没有就退出 */
            break;
        } else if (static_cast<size_t>(len) > iov_[0].iov_len) {
            /*说明第一块区域的数据是发送完毕，再根据多发送了多少调整iov_[1]的取值*/
            iov_[1].iov_base = (uint8_t *)iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if (iov_[0].iov_len) {
                /*区域一的数据发送完毕，回收缓冲空间，并将长度归0*/
                writeBuff_.clearAll();
                iov_[0].iov_len = 0;
            }
        } else {
            /*没有传输完区域一的内容那么就继续传区域一的*/
            iov_[0].iov_base = (uint8_t *)iov_[0].iov_base + len;
            iov_[0].iov_len -= len;
            /*回收对应长度的空间*/
            writeBuff_.hasRead(len);
        }
    } while (bytesNeedWrite() > 0);

    return len;
}

/*
 * 解析http请求数据
 * 本函数实际上时有bug的，不具备处理非完整请求的能力
 * 因为request_.parse(readBuff_)返回false可能是因为请求不完整，需要继续读取http请求数据
 * 但在此函数中却直接返回了400
 * 修改方案：request_.parse()修改为返回解析状态，然后本函数根据解析状态返回true or false
 */
bool HttpConn::process() {
    /*
     * 首先初始化一个httprequest类对象，负责处理请求的事务，
     * 这里不需要每一次都进行初始化，需要保存上次连接的状态，用以指示是否为新的http请求
     * 所以只有当上一次的请求为完成状态时，才回去重新初始化request_成员，以重从开始解析一个http请求
     */
    if (request_.state() == HttpRequest::FINISH) {
        request_.init();
    }
    /*检查读缓存区中是否存在可读数据*/
    if (readBuff_.readableBytes() <= 0) {
        /*小于等于0表示没有数据可读，直接返回false*/
        return false;
    }

    HttpRequest::HTTP_CODE processStatus = request_.parse(readBuff_);
    if (processStatus == HttpRequest::GET_REQUEST) {
        /*使用httprequest类对象解析请求内容，若解析完成，进入回复请求阶段，若失败进入下一个分支*/
        LOG_DEBUG("request path %s", request_.path().c_str());
        /*初始化一个httpresponse对象，负责http应答阶段*/
        response_.init(srcDir, request_.path(), request_.isKeepAlive(), 200);
    } else if (processStatus == HttpRequest::NO_REQUEST) {
        /*请求没有读取完整，应该继续读取请求,返回false让上一层调用函数*/
        return false;
    } else {
        /*其他情况表示解析失败，则返回400错误*/
        response_.init(srcDir, request_.path(), false, 400);
    }

    /*httpresponse负责拼装返回的头部以及需要发送的文件*/
    response_.makeResponse(writeBuff_);
    /*响应头，将写缓冲区赋值给iov_，后面使用writev函数发送至客户端*/
    iov_[0].iov_base = const_cast<char *>(writeBuff_.beginRead());
    iov_[0].iov_len  = writeBuff_.readableBytes();
    iovCnt_          = 1;

    /*如果需要返回服务器的文件内容，且文件内容不为空*/
    if (response_.fileLen() > 0 && response_.file()) {
        /*将文件映射到内存后的地址以及文件长度赋值给iov_变量*/
        iov_[1].iov_base = response_.file();
        iov_[1].iov_len  = response_.fileLen();
        iovCnt_          = 2;
    }
    LOG_DEBUG("filesize: %d, %d to %d", response_.fileLen(), iovCnt_, bytesNeedWrite());

    return true;
}
