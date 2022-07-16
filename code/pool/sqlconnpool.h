#ifndef MY_WEBSERVER_SQLCONNPOLL_H
#define MY_WEBSERVER_SQLCONNPOLL_H

#include <mysql/mysql.h>
#include <semaphore.h>

#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "../logsys/log.h"

/*
 * 单例模式
 */
class SqlConnPool {
private:
    int MAX_CONN_;  // 最大连接数量

    std::mutex mtx_;    // 互斥量
    sem_t      semId_;  // 信号量

    std::queue<MYSQL *> connQue_;  // 连接队列

private:
    SqlConnPool();
    ~SqlConnPool();

public:
    static SqlConnPool *instance();

    void init(const char *host, int port, const char *user, const char *pwd, const char *dbName,
              int connSize);

    MYSQL *getConn();

    void freeConn(MYSQL *sql);

    int getFreeConnCount();

    void closePool();
};

#endif  //MY_WEBSERVER_SQLCONNPOLL_H