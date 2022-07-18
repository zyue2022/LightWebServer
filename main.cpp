/*
 * @Description  : 启动服务器程序
 * @Date         : 2022-07-16 01:14:05
 * @LastEditTime : 2022-07-18 17:21:13
 */
#include "./code/server/webserver.h"

int main() {
    std::tuple<int, int, int, bool, int> webConf;
    /* 服务器端口 触发模式 超时时间 优雅退出 线程池容量*/
    webConf = std::make_tuple(10000, 3, 60000, true, 6);

    std::tuple<int, std::string, std::string, std::string, int> sqlConf;
    /* Mysql端口 用户 密码 数据库名 连接池容量*/
    sqlConf = std::make_tuple(3306, "root", "20191414", "webdb", 12);

    std::tuple<bool, int, int> logConf;
    /* 日志开关 日志等级 异步日志容量 */
    logConf = std::make_tuple(true, 1, 1024);

    auto server = std::make_unique<WebServer>(webConf, sqlConf, logConf);

    server->initServer();
    server->runServer();

    return 0;
}
