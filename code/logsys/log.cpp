#include "log.h"

Log::Log()
    : lineCount_(0),
      today_(0),
      isOpen_(false),  // isOpen_必须初始化为false，因为可能日志没init
      isAsync_(false),
      fp_(nullptr),
      que_(nullptr),
      writeThread_(nullptr),
      curTimeval(nullptr),
      curTm(nullptr) {}

/**
 * @description: 初始化Log类对象
 * @param {int} level           日志级别
 * @param {char} *path          文件路径
 * @param {char} *suffix        文件后缀
 * @param {int} maxQueueSize    异步阻塞队列大小
 */
void Log::init(int level, const char *path, const char *suffix, int maxQueueSize) {
    isOpen_ = true;
    level_  = level;

    /*如果消息队列大于0，说明启用了异步写入log*/
    if (maxQueueSize > 0) {
        /* 开启异步写入 */
        isAsync_ = true;
        if (!que_) {
            /*获取deque_的unique智能指针*/
            que_ = std::make_unique<BlockQueue<std::string>>();
            /*创建异步写线程，获取writeThread_的unique智能指针*/
            writeThread_ = std::make_unique<std::thread>(flushLogThread);
        }
    } else {
        /* 未开启异步写入 */
        isAsync_ = false;
    }

    /*从第一行开始*/
    lineCount_ = 0;
    /*获取当前时间*/
    curTimeval = new timeval();
    curTm      = new tm();
    updateTime();
    /*初始化文件路径以及后缀名*/
    path_   = path;
    suffix_ = suffix;
    /*根据 路径+时间+后缀名创建log文件*/
    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", path_, curTm->tm_year + 1900,
             curTm->tm_mon + 1, curTm->tm_mday, suffix_);

    /*将日期保存到today_变量中*/
    today_ = curTm->tm_mday;

    /*创建文件，使用互斥量保证线程安全*/
    {
        std::lock_guard<std::mutex> locker(mtx_);
        /*确保buff回收完全*/
        if (fp_) {
            flush();
            fclose(fp_);
        }
        /*创建文件目录与文件*/
        fp_ = fopen(fileName, "a");
        /*如果未创建成功，说明没有对应的目录，创建目录后再创建文件即可*/
        if (!fp_) {
            mkdir(path_, 0777);
            fp_ = fopen(fileName, "a");
        }
        assert(fp_ != nullptr);
    }
}

/**
 * @description: 局部静态变量，懒汉单例模式
 */
Log *Log::instance() {
    static Log instance;
    return &instance;
}

/**
 * @description: 更新当前时间
 */
void Log::updateTime() {
    gettimeofday(curTimeval, nullptr);
    time_t tSec = curTimeval->tv_sec;
    curTm       = localtime(&tSec);
}

/**
 * @description:异步写线程的工作函数，静态的
 */
void Log::flushLogThread() { Log::instance()->asyncWrite_(); }

/**
 * @description: 循环从阻塞队列取出数据写入磁盘日志文件，只有队列为空时会休眠等待
 */
void Log::asyncWrite_() {
    std::string str = "";
    while (que_->pop(str)) {
        std::lock_guard<std::mutex> locker(mtx_);
        fputs(str.c_str(), fp_);
    }
}

/**
 * @description: 在析构时将所有log信息写入文件，再关闭写入线程，关闭文件
 */
Log::~Log() {
    if (writeThread_ && writeThread_->joinable()) {
        while (!que_->empty()) {
            que_->wakeupOneConsumer();  // 唤醒一个消费者执行任务
        }
        que_->close();
        writeThread_->join();  // 回收子线程
    }
    if (fp_) {
        std::lock_guard<std::mutex> locker(mtx_);
        flush();
        fclose(fp_);
    }
    isOpen_ = false;
}

/**
 * @description: 将当前的日志数据都写入到文件中
 */
void Log::flush() {
    if (isAsync_) {
        /*如果是异步模式，需要将整个队列都写入*/
        que_->wakeupOneConsumer();
    }
    /*刷新文件缓冲区*/
    fflush(fp_);  // fflush()会强迫将系统缓冲区内的数据写回参数stream 指定的文件中
}

bool Log::isOpen() { return isOpen_; }

int Log::getLevel() { return level_; }

/**
 * @description: 向缓冲区中加入log的级别信息
 * @param {int} level
 */
void Log::appendLogLevelTitle_(int level) {
    switch (level) {
        case 0:
            buff_.append("[debug]: ", 9);
            break;
        case 1:
            buff_.append("[info]:  ", 9);
            break;
        case 2:
            buff_.append("[warn]:  ", 9);
            break;
        case 3:
            buff_.append("[error]: ", 9);
            break;
        default:
            buff_.append("[info]:  ", 9);
            break;
    }
}

/**
 * @description: 按天记录、超行分文件
 */
void Log::adjustFile() {
    /*如果日期变了，也就是到第二天了，或者当前log文件行数达到规定的最大值时都需要创建一个新的log文件*/
    if (today_ != curTm->tm_mday || (lineCount_ && (lineCount_ % MAX_LINES) == 0)) {
        /*最终文件路径存储变量*/
        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        /*根据时间获取文件名*/
        snprintf(tail, 36, "%04d_%02d_%02d", curTm->tm_year + 1900, curTm->tm_mon + 1,
                 curTm->tm_mday);

        if (today_ != curTm->tm_mday) {
            /*日期变化了，拼接 path_ tail suffix_ 获取最新文件名*/
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_);
            /*更新today变量*/
            today_ = curTm->tm_mday;
            /*重置文件行计数变量*/
            lineCount_ = 0;
        } else {
            /*进入到此分支表示文件行数超过了最大行数，需要分出第二个log文件来存储今日的文件*/
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail,
                     (lineCount_ / MAX_LINES), suffix_);
        }

        /*创建文件时上锁，保证线程安全*/
        std::unique_lock<std::mutex> locker(mtx_);
        /*将缓冲区的数据写入到之前的文件中*/
        flush();
        /*关闭上一个文件*/
        fclose(fp_);
        /*根据上面操作得到的文件名创建新的log文件，将指针赋值给fp_变量*/
        fp_ = fopen(newFile, "a");
        assert(fp_ != nullptr);
    }
}

/**
 * @description: 向log文件中写入log信息，写入前践行分类策略
 * @param {int} level
 * @param {char} *format
 */
void Log::write(int level, const char *format, ...) {
    updateTime();
    adjustFile();

    /* ... 使用的可变参数列表*/
    va_list vaList;

    /*上锁，保证线程安全*/
    std::unique_lock<std::mutex> locker(mtx_);

    lineCount_++;

    /*组装信息至缓冲区buff中*/
    int n = snprintf(buff_.beginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                     curTm->tm_year + 1900, curTm->tm_mon + 1, curTm->tm_mday, curTm->tm_hour,
                     curTm->tm_min, curTm->tm_sec, curTimeval->tv_usec);
    buff_.hasWritten(n);
    /*向缓冲区中写入日志级别信息*/
    appendLogLevelTitle_(level);

    /*根据用户传入的参数，向缓冲区添加数据*/
    va_start(vaList, format);  
    // 这里format指形参类型是const char*
    // 可变参数宏通过分析第一个字符串参数中的占位符个数来确定形参的个数；
    // 通过占位符的不同来确定参数类型（%d表示int类型、%s表示char *）
    int m = vsnprintf(buff_.beginWrite(), buff_.writableBytes(), format, vaList);
    va_end(vaList);
    buff_.hasWritten(m);
    /*行尾写入换行符*/
    buff_.append("\n\0", 2);

    /*根据变量选择是否异步写入*/
    if (isAsync_ && que_ && !que_->full()) {
        /*如果以上三个条件都满足，那么进行异步写入*/
        que_->push(buff_.retrieveAllToStr());
    } else {
        /*否则直接写入至文件*/
        fputs(buff_.beginRead(), fp_);
    }
    /*回收所有空间*/
    buff_.clearAll();
}
