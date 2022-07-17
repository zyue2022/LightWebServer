#include "epoller.h"

/**
 * @description: 构造函数中创建并初始化epoll文件描述符与最大监听数量
 * @param {int} maxEvent
 * @return {*}
 */
Epoller::Epoller(int maxEvent) : epollFd_(epoll_create(1)), events_(maxEvent) {
    assert(epollFd_ >= 0 && events_.size() > 0);
}

/**
 * @description: 析构函数中关闭epoll实例
 */
Epoller::~Epoller() { close(epollFd_); }

/**
 * @description: 添加EPOLL监听文件描述符及事件
 * @param {int} fd
 * @param {uint32_t} events
 */
bool Epoller::addFd(int fd, uint32_t events) {
    if (fd < 0) return false;

    epoll_event ev = {0};
    ev.data.fd     = fd;
    ev.events      = events;

    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
}

/**
 * @description: 修改监听事件类型，因为之前设置了EPOLLONESHOT，表示只监听一次
 */
bool Epoller::modFd(int fd, uint32_t events) {
    if (fd < 0) return false;

    epoll_event ev = {0};
    ev.data.fd     = fd;
    ev.events      = events;

    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
}

/**
 * @description: 删除对指定描述符的监听
 * @param {int} fd
 */
bool Epoller::delFd(int fd) {
    if (fd < 0) return false;

    epoll_event ev = {0};

    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &ev);
}

/**
 * @description: 包装epoll等待函数
 * @param {int} timeoutMS 
 * 若timeout == -1 无事件将阻塞，如果timeout大于0时才会设置超时
 */
int Epoller::wait(int timeoutMS) {
    /*因为events_是vector，所以应该取events_[0]数据所在的地址才对*/
    return epoll_wait(epollFd_, &events_[0], static_cast<int>(events_.size()), timeoutMS);
}

int Epoller::getEventFd(size_t i) const {
    assert(i < events_.size() && i >= 0);

    return events_[i].data.fd;
}

uint32_t Epoller::getEvents(size_t i) const {
    assert(i < events_.size() && i >= 0);

    return events_[i].events;
}
