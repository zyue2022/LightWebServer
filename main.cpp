/*
 * @Description  : 启动服务器程序
 * @Date         : 2022-07-16 01:14:05
 * @LastEditTime : 2022-07-21 02:07:31
 */
#include "./code/server/webserver.h"

Json jsonConf() {
    std::string configPath = std::filesystem::current_path();
    configPath += "/serverConf.json";
    std::ifstream ifstrm(configPath);
    assert(ifstrm.is_open());

    std::string json_str;
    std::string err_msg;

    while (ifstrm) {
        std::string line;
        std::getline(ifstrm, line);
        json_str += line + "\n";
    }

    Json json = JSON_API(json_str, err_msg);
    assert(err_msg.empty());

    return json;
}

int main() {
    std::tuple<int, int, int, bool, int> webConf;
    /* 服务器端口 触发模式 超时时间 优雅退出 线程池容量*/
    webConf = std::make_tuple(10000, 3, 60000, true, 6);

    std::tuple<int, std::string, std::string, std::string, int> sqlConf;
    /* Mysql端口 用户 密码 数据库名 连接池容量*/
    sqlConf = std::make_tuple(3306, "root", "12345678", "webdb", 12);

    std::tuple<bool, int, int> logConf;
    /* 日志开关 日志等级 异步日志容量 */
    logConf = std::make_tuple(true, 0, 1024);

    // auto server = std::make_unique<WebServer>(webConf, sqlConf, logConf);

    auto server = std::make_unique<WebServer>(jsonConf());  // 使用json文件配置参数

    /* 初始化并运行服务器 */
    server->initServer();
    server->runServer();

    return 0;
}
