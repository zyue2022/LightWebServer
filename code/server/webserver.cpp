#include "webserver.h"

bool WebServer::isET = false;

/**
 * @description: 构造函数中获取配置参数
 */
WebServer::WebServer(std::tuple<int, int, int, bool, int>                        &webConf,
                     std::tuple<int, std::string, std::string, std::string, int> &sqlConf,
                     std::tuple<bool, int, int>                                  &logConf) {
    std::tie(port_, trigMode_, timeoutMS_, openLinger_, threadNum_) = webConf;
    std::tie(sqlPort_, sqlUser_, sqlPwd_, dbName_, sqlConnNum_)     = sqlConf;
    std::tie(openLog_, logLevel_, logQueSize_) = logConf;
}
/**
 * @description: 初始化各类资源
 */
void WebServer::initServer() {
    // make_unique只是完美转发了它的参数到它要创建的对象的构造函数中去
    epoller_    = std::make_unique<Epoller>();
    timer_      = std::make_unique<HeapTimer>();
    threadPool_ = std::make_unique<ThreadPool>(threadNum_);

    // 当前工作目录是指命令行窗口中运行程序的目录
    /*获取资源目录，返回的是堆内存中的*/
    srcDir_ = getcwd(nullptr, 256);
    assert(srcDir_);
    strncat(srcDir_, "/resources/", 16);

    /*初始化http连接类的静态变量值以及数据库连接池*/
    HttpConn::userCount = 0;
    HttpConn::srcDir    = srcDir_;
    std::string host_   = "localhost";
    SqlConnPool::instance()->init(host_, sqlPort_, sqlUser_, sqlPwd_, dbName_, sqlConnNum_);

    /*根据参数设置连接事件与监听事件的出发模式LT或ET*/
    initEventMode_();

    /*初始化监听套接字*/
    if (!initListenFd_()) {
        isClose_ = true;
    }

    /*日志开关*/
    if (openLog_) {
        /*初始化LOG类设置*/
        Log::instance()->init(logLevel_, "./log", ".log", logQueSize_);
        if (isClose_) {
            LOG_ERROR("====================Server init error!===================");
        } else {
            LOG_INFO("================Server init===================");
            LOG_INFO("Port: %d, OpenLinger: %s", port_, openLinger_ ? "true" : "false");
            LOG_INFO("Listen Mode: %s, Conn Mode: %s", (listenEvent_ & EPOLLET ? "ET" : "LT"),
                     (connEvent_ & EPOLLET ? "ET" : "LT"));
            LOG_INFO("Log level: %d", logLevel_);
            LOG_INFO("srcDir: %s", srcDir_);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", sqlConnNum_, threadNum_);
        }
    }
}

/**
 * @description: 析构函数中关闭连接以及申请的资源
 */
WebServer::~WebServer() {
    isClose_ = true;
    close(listenFd_);
    free(srcDir_);
    SqlConnPool::instance()->closePool();
}

/**
 * @description:  初始化事件工作模式
 *                  EPOLLIN：表示对应的文件描述符可以读（包括对端SOCKET正常关闭）
 *                  EPOLLOUT：表示对应的文件描述符可以写
 *                  EPOLLERR：表示对应的文件描述符发生错误
 *                  EPOLLHUP：表示对应的文件描述符被挂断；读写关闭
 *                  EPOLLRDHUP 表示读关闭
 *                  EPOLLET：将EPOLL设为边缘触发(Edge Triggered)模式，这是相对于水平触发(Level Triggered)而言的
 * E                POLLONESHOT：只监听一次事件，当监听完后如果还需要监听这个socket，就要再次把这个socket加入到EPOLL队列里
 * @param {int} trigMode
 */
void WebServer::initEventMode_() {
    listenEvent_ = EPOLLHUP;
    /*EPOLLONESHOT，为了保证当前连接在同一时刻只被一个线程处理*/
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;
    /*根据触发模式设置对应选项*/
    switch (trigMode_) {
        case 0:
            // 默认LT
            break;
        case 1:
            /*连接模式为ET，边沿触发*/
            connEvent_ |= EPOLLET;
            break;
        case 2:
            /*监听模式为ET，边沿触发*/
            listenEvent_ |= EPOLLET;
            break;
        case 3:
            /*处理模式皆为边沿触发*/
            connEvent_ |= EPOLLET;
            listenEvent_ |= EPOLLET;
            break;
        default:
            connEvent_ |= EPOLLET;
            listenEvent_ |= EPOLLET;
            break;
    }
    /*若连接事件为ET模式，那么设置Http类中的标记isET为true*/
    HttpConn::isET  = (connEvent_ & EPOLLET);
    WebServer::isET = (listenEvent_ & EPOLLET);
}

/**
 * @description: 创建本机监听描述符
 * @param {bool} openLinger 对于残存在套接字发送队列中的数据：丢弃或者将发送至对端，优雅关闭连接
 * @return {bool}
 */
bool WebServer::initListenFd_() {
    int ret = 0;

    /*合法性检查*/
    if (port_ > 65535 || port_ < 1024) {
        LOG_ERROR("Port: %d exceed range!", port_);
        return false;
    }

    /*设置监听地址*/
    struct sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port_);

    /*创建监听套接字*/
    listenFd_ = socket(PF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        LOG_ERROR("Create socket error!");
        return false;
    }

    /*设置openLinger项*/
    struct linger optLinger = {0};
    if (openLinger_) {
        /*优雅关闭: 直到所剩数据发送完毕或超时*/
        optLinger.l_onoff  = 1;
        optLinger.l_linger = 1;
    }

    /*设置连接选项，是否优雅关闭连接*/
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if (ret < 0) {
        LOG_ERROR("Init linger error!");
        close(listenFd_);
        return false;
    }

    /* 端口复用，SO_REUSEADDR 立即开启这个端口，不用管之前关闭连接后的2MSL*/
    /* 只有最后一个套接字会正常接收数据。 */
    int optVal = 1;
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void *)&optVal, sizeof(int));
    if (ret < 0) {
        LOG_ERROR("Set socket reuse address error!");
        close(listenFd_);
        return false;
    }

    /*绑定套接字监听地址*/
    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        LOG_ERROR("Bind socket error!");
        close(listenFd_);
        return false;
    }

    /*开始监听，socket可以排队的最大连接数最大5个*/
    ret = listen(listenFd_, 5);
    if (ret < 0) {
        LOG_ERROR("Listen port: %d error!", port_);
        close(listenFd_);
        return false;
    }

    /*将监听描述符加入到epoll的监听事件中，监听EPOLLIN读事件*/
    bool res = epoller_->addFd(listenFd_, listenEvent_ | EPOLLIN);
    if (!res) {
        LOG_ERROR("Add epoll listen error!");
        close(listenFd_);
        return false;
    }

    /*设置监听事件为非阻塞的*/
    setFdNonblock(listenFd_);
    LOG_INFO("Server init success! Server port is: %d", port_);

    return true;
}

/**
 * @description: 设置文件描述符为非阻塞
 * @param {int} fd
 */
int WebServer::setFdNonblock(int fd) {
    assert(fd > 0);
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

/**
 * @description: 向客户端发送错误消息
 * @param {int} fd
 * @param {char} *info
 */
void WebServer::sendError_(int fd, const char *info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if (ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    /*关闭连接*/
    close(fd);
}

/**
 * @description: 更新计时器中的过期时间
 */
void WebServer::extentTime_(HttpConn *client) {
    assert(client);
    if (timeoutMS_ > 0) {
        timer_->adjust(client->getFd(), timeoutMS_);
    }
}

/**
 * @description: 关闭客户端连接，主要是移除epoll监听、关闭连接类对象
 * @param {HttpConn} *client
 */
void WebServer::closeConn_(HttpConn *client) {
    assert(client);
    LOG_INFO("Client[%d] quit!", client->getFd());
    epoller_->delFd(client->getFd());
    client->closeConn();
}

/**
 * @description: 初始化httpconn类对象，添加epoll监听事件和对应连接的计时器
 * @param {int} fd
 * @param {sockaddr_in} addr
 */
void WebServer::addClient_(int fd, sockaddr_in addr) {
    assert(fd > 0);
    /*初始化httpconn类对象*/
    users_[fd].init(fd, addr);

    /*添加epoll监听EPOLLIN事件，连接设置为非阻塞*/
    epoller_->addFd(fd, EPOLLIN | connEvent_);
    setFdNonblock(fd);

    if (timeoutMS_ > 0) {
        /*若设置了超时事件，则需要向定时器里添加这一项*/
        // 使用bind绑定到成员函数时，即使成员函数不需参数，也要将this绑定在第一个参数
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::closeConn_, this, &users_[fd]));
    }

    LOG_INFO("Client[%d] in!", users_[fd].getFd());
}

/**
 * @description: 处理客户端连接事件
 */
void WebServer::dealListen_() {
    struct sockaddr_in addr;
    socklen_t          len = sizeof(addr);

    /*使用do-while很巧妙，因为无论如何都会进入一次循环体，如果监听事件设置为LT模式，则只会调用一次accept与addClient方法
     * 若监听事件是ET模式，则会将连接一次性接受完，直到accept返回-1，表示当前没有连接了
     */
    do {
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);
        if (fd < 0) {
            return;
        } else if (HttpConn::userCount >= MAX_FD) {
            /*当前连接数太多，超过了预定义了最大数量，向客户端发送错误信息*/
            sendError_(fd, "Server busy!");
            LOG_WARN("Clients is full and reject a connectin!");
            return;
        }
        /*添加客户事件*/
        addClient_(fd, addr);
    } while (WebServer::isET);
}

/**
 * @description: 读取socket传来的数据，读取后调用onProcess函数处理
 * @param {HttpConn} *client
 */
void WebServer::onRead_(HttpConn *client) {
    assert(client);
    int ret       = -1;
    int readErrno = 0;

    /*调用httpconn类的read方法，读取数据*/
    ret = client->read(&readErrno);
    if (ret < 0 && readErrno != EAGAIN) {
        /*若返回值小于0，且信号不为EAGAIN说明发生了错误*/
        closeConn_(client);
        return;
    }
    /*调用onProcess函数解析数据*/
    onProcess_(client);
}

/**
 * @description: 处理http报文请求
 * @param {HttpConn} *client
 */
void WebServer::onProcess_(HttpConn *client) {
    if (client->process()) {
        /*成功处理则将epoll在该文件描述符上的监听事件改为EPOLLOUT写事件*/
        epoller_->modFd(client->getFd(), connEvent_ | EPOLLOUT);
    } else {
        /*未成功处理，说明数据还没有读完，需要继续使用epoll监听该连接上的EPOLLIN读事件*/
        epoller_->modFd(client->getFd(), connEvent_ | EPOLLIN);
    }
}

/**
 * @description: 向对应的socket发送响应报文数据
 * @param {HttpConn} *client
 */
void WebServer::onWrite_(HttpConn *client) {
    assert(client);
    int ret        = -1;
    int writeErrno = 0;

    /*调用httpconn类的write方法向socket发送数据*/
    ret = client->write(&writeErrno);
    if (client->bytesNeedWrite() == 0) {
        /*完成传输，检查客户端是否设置了长连接字段*/
        if (client->isKeepAlive()) {
            /*如果客户端设置了长连接，那么重新注册epoll的EPOLLIN事件*/
            epoller_->modFd(client->getFd(), connEvent_ | EPOLLIN);
            return;
        }
    } else if (ret < 0) {
        /*若是缓冲区满了，errno会返回EAGAIN*/
        if (writeErrno == EAGAIN) {
            /*若返回值小于0，且信号为EAGAIN说明数据还没有发送完，重新在EPOLL上注册该连接的EPOLLOUT事件*/
            epoller_->modFd(client->getFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    /*其余情况，关闭连接*/
    closeConn_(client);
}

/**
 * @description: 读取一个客户端连接发送来的数据，调整当前连接的过期时间，向线程池中添加读数据的任务
 * @param {HttpConn} *client
 */
void WebServer::dealRead_(HttpConn *client) {
    assert(client);
    extentTime_(client);
    threadPool_->addTask(std::bind(&WebServer::onRead_, this, client));
}

/**
 * @description: 处理连接中的发送数据事件，调整当前连接的过期时间，向线程池中添加发送数据的任务
 * @param {HttpConn} *client
 */
void WebServer::dealWrite_(HttpConn *client) {
    assert(client);
    extentTime_(client);
    threadPool_->addTask(std::bind(&WebServer::onWrite_, this, client));
}

/**
 * @description: 启动服务器，主线程工作
 */
void WebServer::runServer() {
    int timeMS = -1;
    if (!isClose_) {
        LOG_INFO("================Server run====================");
    }

    /*根据不同的事件调用不同的函数*/
    while (!isClose_) {
        /*每开始一轮的处理事件时，若设置了超时时间，那就处理一下超时事件*/
        if (timeoutMS_ > 0) {
            /*先删除超时节点，再获取最近的超时时间*/
            timeMS = timer_->getNextTick();
        }
        /*epoll等待事件的唤醒，等待时间为最近一个连接会超时的时间*/
        int eventCount = epoller_->wait(timeMS);
        for (int i = 0; i < eventCount; i++) {
            /*获取对应文件描述符与epoll事件*/
            int      fd     = epoller_->getEventFd(i);
            uint32_t events = epoller_->getEvents(i);

            /*根据不同情况进入不同分支*/
            if (fd == listenFd_) {
                /* 新客户端连接 */
                dealListen_();
            } else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                /*表示连接出现问题，需要关闭该连接*/
                assert(users_.count(fd) > 0);
                closeConn_(&users_[fd]);
            } else if (events & EPOLLIN) {
                /*若epoll事件为EPOLLIN，表示有对应套接字收到数据，需要读取出来*/
                assert(users_.count(fd) > 0);
                dealRead_(&users_[fd]);
            } else if (events & EPOLLOUT) {
                /*若epoll事件为EPOLLOUT，表示返回给客户端的数据已准备好，需要向对应套接字连接发送数据*/
                assert(users_.count(fd) > 0);
                dealWrite_(&users_[fd]);
            } else {
                /*其余事件皆为错误，向log文件写入该事件*/
                LOG_ERROR("Unexpected event");
            }
        }
    }
}
