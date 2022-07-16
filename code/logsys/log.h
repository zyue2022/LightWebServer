/*
 * @Description  : 日志记录模块，按天按行按级别记录，同步或异步记录，单例模式
 * @Date         : 2022-07-16 01:14:05
 * @LastEditTime : 2022-07-16 21:32:55
 */
#ifndef MY_WEBSERVER_LOG_H
#define MY_WEBSERVER_LOG_H

#include <assert.h>
#include <stdarg.h> /*va_start va_end*/
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <mutex>
#include <string>
#include <thread>

#include "../buffer/buffer.h"
#include "blockqueue.h"

class Log {
private:
    static const int LOG_PATH_LEN = 256;    // 最大log文件路径长度
    static const int LOG_NAME_LEN = 256;    // 最大log文件名长度
    static const int MAX_LINES    = 50000;  // log文件最大行数，超过就单独划分文件

    const char *path_;    // log文件路径
    const char *suffix_;  // 文件后缀名

    int  lineCount_;  // 当前log文件行数
    int  today_;      // 本月的哪一天
    int  level_;      // 日志级别
    bool isOpen_;     // 是否开启
    bool isAsync_;    // 是否异步模式

    FILE  *fp_;    // log文件流
    Buffer buff_;  // 缓冲区

    std::unique_ptr<BlockQueue<std::string>> que_;          // log信息阻塞队列
    std::unique_ptr<std::thread>             writeThread_;  // 写入日志的线程
    std::mutex                               mtx_;          // 互斥量

    timeval *curTimeval;  // 当前时间，为了获取毫秒
    tm      *curTm;       // 当前时间结构体

private:
    Log();
    virtual ~Log();

    void appendLogLevelTitle_(int level);

    void adjustFile();

    void updateTime();

    void        asyncWrite_();
    static void flushLogThread();  // 异步时写线程工作函数，必须是静态的

public:
    void init(int level = 1, const char *path = "./log", const char *suffix = ".log",
              int maxQueueSize = 1024);

    static Log *instance();

    void write(int level, const char *format, ...);

    void flush();

    int  getLevel();
    bool isOpen();
};

/**
 * @description: 定义log日志相关的宏，按照日志等级写入日志信息；为什么写成do while(0); ?
 * @return {*}
 */
#define LOG_BASE(level, format, ...)                     \
    do {                                                 \
        Log *log = Log::instance();                      \
        if (log->isOpen() && log->getLevel() <= level) { \
            log->write(level, format, ##__VA_ARGS__);    \
            log->flush();                                \
        }                                                \
    } while (0);

#define LOG_DEBUG(format, ...)             \
    do {                                   \
        LOG_BASE(0, format, ##__VA_ARGS__) \
    } while (0);

#define LOG_INFO(format, ...)              \
    do {                                   \
        LOG_BASE(1, format, ##__VA_ARGS__) \
    } while (0);

#define LOG_WARN(format, ...)              \
    do {                                   \
        LOG_BASE(2, format, ##__VA_ARGS__) \
    } while (0);

#define LOG_ERROR(format, ...)             \
    do {                                   \
        LOG_BASE(3, format, ##__VA_ARGS__) \
    } while (0);

#endif  //MY_WEBSERVER_LOG_H
