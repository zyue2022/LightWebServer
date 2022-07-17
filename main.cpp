/*
 * @Description  : 启动服务器程序
 * @Date         : 2022-07-16 01:14:05
 * @LastEditTime : 2022-07-17 22:07:35
 */
#include "./code/server/webserver.h"

int main() {
    WebServer* obj = new WebServer(10000, 3, 60000, true, /* 端口 触发模式 超时时间 优雅退出 */
                                   3306, "root", "20191414", "webdb", /* Mysql配置 */
                                   12, 6,          /* 连接池容量 线程池容量 */
                                   true, 1, 1024); /* 日志开关 日志等级 异步日志容量 */
    obj->run();

    return 0;
}
