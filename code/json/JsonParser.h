/*
 * @Description  : 字符串解析类，返回解析字符串后得到的Json对象
 * @Date         : 2022-07-19 14:48:31
 * @LastEditTime : 2022-07-20 21:16:31
 */
#ifndef JSON_PARSE_H
#define JSON_PARSE_H

#include <cassert>
#include <cmath>
#include <cstring>

#include "JsonException.h"

namespace lightJson {

bool is1to9(char ch);
bool is0to9(char ch);

class Json;

class JsonParser {
private:
    const char* _start;
    const char* _curr;

public:
    explicit JsonParser(const char* cstr) : _start(cstr), _curr(cstr) {}
    explicit JsonParser(const std::string& content) : JsonParser(content.c_str()) {}

    JsonParser(const JsonParser&)            = delete;
    JsonParser& operator=(const JsonParser&) = delete;

    ~JsonParser() = default;

    Json parseJson();

private:
    Json parseValue();

    Json parseLiteral(const std::string& literal);
    Json parseNumber();
    Json parseString();
    Json parseArray();
    Json parseObject();

    void jumpSpace();

    unsigned    parse4hex();
    std::string encodeUTF8(unsigned u);
    std::string parseRawString();
};

}  // namespace lightJson

#endif