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
    /*定义一个结构体，保存相关变量*/
    struct Pool {
        std::mutex                        mtx;       // 互斥量
        std::condition_variable           cond;      // 条件变量
        bool                              isClosed;  // 标志变量，表示是否关闭线程池
        std::queue<std::function<void()>> tasks;     // 任务队列
    };

    std::shared_ptr<Pool> pool_;  // 使用动态申请的堆内存空间，使用shareptr管理

public:
    /*
     * 构造函数中根据传入的参数构建线程池，并设置线程分离
     */
    explicit ThreadPool(size_t threadCount = std::thread::hardware_concurrency())
        : pool_(std::make_shared<Pool>()) {
        assert(threadCount > 0);
        for (size_t i = 0; i < threadCount; i++) {
            /*使用lambda表达式作为线程的工作函数*/
            std::thread([pool = pool_] {
                std::unique_lock<std::mutex> locker(pool->mtx);
                while (true) {
                    if (!pool->tasks.empty()) {
                        /*任务队列不为空，进入本段代码，采用右值的方式取出任务，任务取出成功，解锁，执行完任务重新获取锁*/
                        auto task = std::move(pool->tasks.front());
                        pool->tasks.pop();
                        locker.unlock();
                        /*执行任务*/
                        task();
                        locker.lock();
                    } else if (pool->isClosed) {
                        /*说明线程池收到了关闭信号，直接跳出循环*/
                        break;
                    } else {
                        /*到了此分支，说明任务队列为空，此时条件变量等待，wait方法会自动释放锁，当收到notify信号时重新尝试获取锁*/
                        pool->cond.wait(locker);
                    }
                }
            }).detach();  // 设置线程分离
        }
    }

    /*
     * 让编译器生成一个默认的移动构造函数
     */
    ThreadPool(ThreadPool &&) = default;

    /*
     * 析构函数中将pool_->isClosed = true;标志置为false，这样detach的线程会自动关闭
     */
    ~ThreadPool() {
        if (static_cast<bool>(pool_)) {
            {
                std::lock_guard<std::mutex> locker(pool_->mtx);
                pool_->isClosed = true;
            }
            /*唤醒所有线程，这个线程池的逻辑是执行完了任务队列中的所有任务后才会退出线程*/
            pool_->cond.notify_all();
        }
    }

    /*
     * 类成员模板函数，参数自动推断，向任务队列中添加任务
     * 这里可以设置一个最大任务数量，若超过此数量，禁止向队列加入任务
     */
    template <class F>
    void addTask(F &&task) {
        /*利用RAII自动加锁解锁下面这块作用域*/
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);
            /*完美转发*/
            pool_->tasks.emplace(std::forward<F>(task));
        }
        /*加入一个任务，唤醒一个线程*/
        pool_->cond.notify_one();
    }
};

#endif  //MY_WEBSERVER_THREADPOOL_H
