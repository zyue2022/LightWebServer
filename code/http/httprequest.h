/*
 * @Description  : HTTP请求的解析类
 * @Date         : 2022-07-16 01:14:05
 * @LastEditTime : 2022-07-17 00:52:46
 */
#ifndef MY_WEBSERVER_HTTPREQUEST_H
#define MY_WEBSERVER_HTTPREQUEST_H

#include <errno.h>
#include <mysql/mysql.h>

#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "../buffer/buffer.h"
#include "../logsys/log.h"
#include "../pool/sqlconnRAII.h"

class HttpRequest {
public:
    /*指示解析到请求头的哪一部分的枚举变量*/
    enum PARSE_STATE { REQUEST_LINE, HEADER, BODY, FINISH };

    /*指示解析结果的枚举变量*/
    enum HTTP_CODE {
        NO_REQUEST = 0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };

private:
    PARSE_STATE state_;                           // 状态机状态
    std::string method_, path_, version_, body_;  // 请求头中的信息

    /* 以键值对的方式保存请求头、请求体中的信息 */
    std::unordered_map<std::string, std::string> header_;
    std::unordered_map<std::string, std::string> post_;

    // 静态常量
    static const std::unordered_set<std::string>      DEFAULT_HTML;      // 各类型页面的地址
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;  // 默认的HTML标签

public:
    HttpRequest();
    ~HttpRequest() = default;  // 生成默认析构函数

    void init();

    HTTP_CODE parse(Buffer &buff);

    PARSE_STATE state() const;
    std::string path() const;
    std::string method() const;
    std::string version() const;
    std::string getPost(const std::string &key) const;

    bool isKeepAlive() const;

private:
    static int convertHex(char ch);

    static bool userVerify(const std::string &name, const std::string &pwd, bool isLogin);

    bool parseRequestLine_(const std::string &line);
    void parseHeader_(const std::string &line);
    bool parseBody_(const std::string &line);
    void parsePath_();
    bool parsePost_();
    void parseFromUrlencoded_();
};

#endif  //MY_WEBSERVER_HTTPREQUEST_H
