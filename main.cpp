#include <unistd.h>

#include "./code/server/webserver.h"

int main() {
    WebServer server(10000, 3, 60000, false, /* 端口 ET模式 超时时间 优雅退出 */
                     3306, "root", "20191414", "webdb", /* Mysql配置 */
                     12, 6,                             /* 连接池数量 线程池数量 */
                     true, 0, 1024); /* 日志开关 日志等级 异步日志容量 */
    server.start();
}
