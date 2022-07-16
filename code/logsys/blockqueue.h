/*
 * @Description  : 封装生产者消费者模型，实现线程安全的阻塞队列
 * @Date         : 2022-07-16 01:14:05
 * @LastEditTime : 2022-07-16 20:19:46
 */
#ifndef MY_WEBSERVER_BLOCKQUEUE_H
#define MY_WEBSERVER_BLOCKQUEUE_H

#include <assert.h>
#include <sys/time.h>

#include <condition_variable>
#include <mutex>
#include <queue>

template <class T>
class BlockQueue {
private:
    std::queue<T> que_;  // 共享队列
    std::mutex    mtx_;  // 互斥量

    std::condition_variable condConsumer_;  // 消费者条件变量
    std::condition_variable condProducer_;  // 生产者条件变量

    size_t capacity_;  // 最大容量
    bool   isClose_;   // 是否关闭

public:
    explicit BlockQueue(size_t MaxCapacity = 1000);

    ~BlockQueue();

    void clear();
    void close();

    size_t size();
    size_t capacity();

    bool empty();
    bool full();

    void push(const T &item);

    bool pop(T &item);
    bool pop(T &item, int timeout);

    void wakeupOneConsumer();
};

template <typename T>
BlockQueue<T>::BlockQueue(size_t MaxCapacity) : capacity_(MaxCapacity), isClose_(false) {
    assert(MaxCapacity > 0);
}

template <typename T>
BlockQueue<T>::~BlockQueue() {
    close();
}

/**
 * @description: 用在析构函数中，释放资源，关闭前唤醒所有等待的事件
 */
template <typename T>
void BlockQueue<T>::close() {
    clear();
    isClose_ = true;
    condProducer_.notify_all();
    condConsumer_.notify_all();
}

/**
 * @description: 清空队列
 */
template <typename T>
void BlockQueue<T>::clear() {
    std::lock_guard<std::mutex> locker(mtx_);
    std::queue<T>               tmp;
    std::swap(tmp, que_);
}

/**
 * @description: 唤醒一个消费者
 */
template <typename T>
void BlockQueue<T>::wakeupOneConsumer() {
    condConsumer_.notify_one();
}

template <typename T>
size_t BlockQueue<T>::size() {
    std::lock_guard<std::mutex> locker(mtx_);
    return que_.size();
}

template <typename T>
size_t BlockQueue<T>::capacity() {
    std::lock_guard<std::mutex> locker(mtx_);
    return capacity_;
}

template <typename T>
bool BlockQueue<T>::empty() {
    std::lock_guard<std::mutex> locker(mtx_);
    return que_.empty();
}

template <typename T>
bool BlockQueue<T>::full() {
    std::lock_guard<std::mutex> locker(mtx_);
    return que_.size() >= capacity_;
}

/**
 * @description: 向队尾加入一个元素
 */
template <typename T>
void BlockQueue<T>::push(const T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    /*条件变量控制，如果元素超过了容量，需要阻塞等待消费者消费后才能加入*/
    while (que_.size() >= capacity_) {
        /*生产者等待*/
        condProducer_.wait(locker);
    }
    que_.push(item);
    /*唤醒一个消费者*/
    condConsumer_.notify_one();
}

/**
 * @description: 在队头弹出一个元素
 */
template <typename T>
bool BlockQueue<T>::pop(T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    /*如果队列为空，消费者条件变量等待，若关闭变量isClose为true这返回false*/
    while (que_.empty()) {
        condConsumer_.wait(locker);
        if (isClose_) {
            return false;
        }
    }
    item = que_.front();
    que_.pop();
    /*唤醒一个生产者*/
    condProducer_.notify_one();

    return true;
}

/**
 * @description: 在队头弹出一个元素，设有阻塞时间
 */
template <typename T>
bool BlockQueue<T>::pop(T &item, int timeout) {
    std::unique_lock<std::mutex> locker(mtx_);
    while (que_.empty()) {
        if (condConsumer_.wait_for(locker, std::chrono::seconds(timeout)) ==
            std::cv_status::timeout) {
            return false;
        }
        if (isClose_) {
            return false;
        }
    }
    item = que_.front();
    que_.pop();
    /*唤醒一个生产者*/
    condProducer_.notify_one();

    return true;
}
#endif  //MY_WEBSERVER_BLOCKQUEUE_H
