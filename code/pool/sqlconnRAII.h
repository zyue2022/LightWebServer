/*
 * @Description  : RAII机制封装MySQL连接类，用于从连接池取出连接
 * @Date         : 2022-07-16 01:14:06
 * @LastEditTime : 2022-07-16 23:39:30
 */
#ifndef MY_WEBSERVER_SQLCONNRAII_H
#define MY_WEBSERVER_SQLCONNRAII_H

#include "sqlconnpool.h"

class SqlConnRAII {
private:
    MYSQL       *sql_;       // 数据库连接
    SqlConnPool *connpool_;  // 连接池

public:
    /**
     * @description: 构造函数中获取连接
     * 因为这里需要修改指针的值，所以需要拆入二级指针，也可以用指针的引用
     */    
    SqlConnRAII(MYSQL **sql, SqlConnPool *connpool) {
        assert(connpool);
        *sql      = connpool->getConn();
        sql_      = *sql;
        connpool_ = connpool;
    }

    /**
     * @description:  析构函数中自动释放
     */    
    ~SqlConnRAII() {
        if (sql_) {
            connpool_->freeConn(sql_);
        }
    }
};

#endif  //MY_WEBSERVER_SQLCONNRAII_H
