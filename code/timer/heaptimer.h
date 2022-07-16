#ifndef MY_WEBSERVER_HEAPTIMER_H
#define MY_WEBSERVER_HEAPTIMER_H

#include <arpa/inet.h>
#include <assert.h>
#include <time.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <queue>
#include <unordered_map>

#include "../logsys/log.h"

// functional对象，接受一个bind函数绑定的函数对象
typedef std::function<void()> TimeoutCallBack;  // 回调函数

typedef std::chrono::milliseconds          MS;         // 毫秒
typedef std::chrono::high_resolution_clock Clock;      // 获取时间的类
typedef Clock::time_point                  TimeStamp;  // 时间戳

/*
 * 定义的绑定文件描述符，超时时间，删除函数的结构体
 */
struct TimerNode {
    int       fd;       // 节点对应的文件描述符
    TimeStamp expires;  // 超时时间
    TimeoutCallBack cb;  // 回调函数，这里接受的是删除节点后的对应操作，WebServer::CloseConn_

    // 重载比较运算符，方便比较时间
    bool operator<(const TimerNode &t) { return expires < t.expires; }
};

class HeapTimer {
private:
    std::vector<TimerNode> heap_;  // 使用vector实现的堆

    // key : fd, value : 节点在数组中下标
    std::unordered_map<int, size_t> ref_;  // 使用hash方便确定节点是否存在，O(1)，

    void del_(size_t i);
    void siftup_(size_t i);
    bool siftdown_(size_t index, size_t n);
    void swapNode_(size_t i, size_t j);

public:
    HeapTimer();
    ~HeapTimer();

    void adjust(int fd, int newExpires);

    void add(int fd, int timeout, const TimeoutCallBack &cb);

    void pop();

    void tick();
    int  getNextTick();

    void doWork(int fd);
};

#endif  //MY_WEBSERVER_HEAPTIMER_H
