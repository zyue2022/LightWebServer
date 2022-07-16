#ifndef MY_WEBSERVER_THREADPOOL_H
#define MY_WEBSERVER_THREADPOOL_H

#include <assert.h>

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

class ThreadPool {
private:
    std::mutex                        mtx;          // 互斥量
    std::condition_variable           cond;         // 条件变量
    std::atomic<bool>                 isClosed;     // 是否关闭线程池
    std::queue<std::function<void()>> tasks;        // 任务队列
    std::size_t                       threadCount;  // 线程数目

public:
    explicit ThreadPool(std::size_t count = std::thread::hardware_concurrency());

    ~ThreadPool();

    template <class F>
    void addTask(F&& task);
};

/**
 * @description: 构造函数中构建线程池，使用lambda表达式作为线程的工作函数，并设置线程分离
 */
inline ThreadPool::ThreadPool(std::size_t count) : isClosed(false), threadCount(count) {
    assert(threadCount > 0);
    for (std::size_t i = 0; i < threadCount; i++) {
        std::thread([this] {
            std::unique_lock<std::mutex> locker(mtx);
            while (true) {
                if (!tasks.empty()) {
                    /*任务队列不为空，进入本段代码，采用右值的方式取出任务，任务取出成功，解锁，执行完任务重新获取锁*/
                    auto task = std::move(tasks.front());
                    tasks.pop();
                    locker.unlock();
                    /*执行任务*/
                    task();
                    locker.lock();
                } else if (isClosed) {
                    /*说明线程池收到了关闭信号，直接跳出循环*/
                    break;
                } else {
                    /*到了此分支，说明任务队列为空，此时条件变量等待，会自动释放锁，当收到notify信号时重新尝试获取锁*/
                    cond.wait(locker);
                }
            }
        }).detach();  // 设置线程分离
    }
}

/**
 * @description: 析构，执行完了任务队列中的所有任务后才会退出线程
 */
inline ThreadPool::~ThreadPool() {
    isClosed = true;
    /*唤醒所有线程*/
    cond.notify_all();
}

/**
 * @description: 参数自动推断，向任务队列中添加任务
 * TODO:         这里可以设置一个最大任务数量，若超过此数量，禁止向队列加入任务
 */
template <class F>
void ThreadPool::addTask(F&& task) {
    {
        std::unique_lock<std::mutex> locker(mtx);
        /*完美转发*/
        tasks.emplace(std::forward<F>(task));
    }
    /*加入一个任务，唤醒一个线程*/
    cond.notify_one();
}

#endif  //MY_WEBSERVER_THREADPOOL_H
