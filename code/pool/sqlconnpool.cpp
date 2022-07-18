#include "sqlconnpool.h"

SqlConnPool::~SqlConnPool() { closePool(); }

/**
 * @description: 关闭数据库连接池，被析构函数调用
 */
void SqlConnPool::closePool() {
    std::lock_guard<std::mutex> locker(mtx_);
    while (!connQue_.empty()) {
        auto sql = connQue_.front();
        connQue_.pop();
        mysql_close(sql);
    }
    mysql_library_end();
}

/**
 * @description: 懒汉单例模式，局部静态变量
 */
SqlConnPool *SqlConnPool::instance() {
    static SqlConnPool connPool;
    return &connPool;
}

/**
 * @description: 初始化数据库连接池
 */
void SqlConnPool::init(std::string &host, int port, std::string &user, std::string &pwd,
                       std::string &dbName, int connSize) {
    assert(connSize > 0);
    for (int i = 0; i < connSize; i++) {
        /*初始化以及配置一个sql连接*/
        MYSQL *sql = nullptr;
        sql        = mysql_init(sql);
        if (!sql) {
            LOG_ERROR("Mysql init error!");
            assert(sql);
        }
        sql = mysql_real_connect(sql, host.c_str(), user.c_str(), pwd.c_str(), dbName.c_str(), port,
                                 nullptr, 0);
        if (!sql) {
            LOG_ERROR("MySql Connect error!");
            assert(sql);
        }
        /*将sql连接入队*/
        connQue_.push(sql);
    }
    MAX_CONN_ = connSize;
    /*初始化信号量值为数据池连接个数*/
    sem_init(&semId_, 0, MAX_CONN_);
}

/**
 * @description: 取出数据库连接池中的一个连接
 */
MYSQL *SqlConnPool::getConn() {
    MYSQL *sql = nullptr;
    if (connQue_.empty()) {
        LOG_WARN("SqlConnPool busy!");
        return nullptr;
    }
    /*将信号量减1*/
    sem_wait(&semId_);

    {
        /*RAII的应用，加锁局部块作用域*/
        std::lock_guard<std::mutex> locker(mtx_);
        sql = connQue_.front();
        connQue_.pop();
    }
    assert(sql);

    return sql;
}

/**
 * @description: 释放一个数据库连接，重新将连接入池
 * @param {MYSQL} *sql
 */
void SqlConnPool::freeConn(MYSQL *sql) {
    assert(sql);
    std::lock_guard<std::mutex> locker(mtx_);
    connQue_.push(sql);
    /*将信号量加1*/
    sem_post(&semId_);
}

/**
 * @description: 获取可用连接的数量
 */
int SqlConnPool::getFreeConnCount() {
    std::lock_guard<std::mutex> locker(mtx_);

    return connQue_.size();
}
