/*
 * @Description  : Json类，保存Json类型，提供对外接口
 * @Date         : 2022-07-19 14:48:31
 * @LastEditTime : 2022-07-20 22:06:37
 */
#ifndef JSON_H
#define JSON_H

#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "JsonException.h"
#include "JsonParser.h"

namespace lightJson {
/* 外部调用接口，生成json对象 */
Json JSON_API(const std::string& content, std::string& errMsg);

/* Json对象类型，限定作用域的枚举类型 */
enum class JsonType { Null, Bool, Number, String, Array, Object };

using jsonArray  = std::vector<Json>;                      // Json数据类型中的 Array
using jsonObject = std::unordered_map<std::string, Json>;  // Json数据类型中的 Object

/* 变体类型, json对象的值类型就是其中之一*/
using type = std::variant<std::nullptr_t, bool, double, std::string, jsonArray, jsonObject>;

class Json {
private:
    /* 一个Json对象若是array或object类型，则含有其它Json对象，是树形结构，所以用指针管理 */
    std::unique_ptr<type> valuePtr_;

    const type& getVariant() const { return *valuePtr_; }

public:
    /* 构造函数，禁止隐形转换 */

    /* 有参构造函数，顶层 */
    explicit Json(std::nullptr_t) : valuePtr_(std::make_unique<type>(nullptr)) {}
    explicit Json(bool val) : valuePtr_(std::make_unique<type>(val)) {}
    explicit Json(double val) : valuePtr_(std::make_unique<type>(val)) {}
    /* 有参构造函数，底层 */
    explicit Json(const std::string& val) : valuePtr_(std::make_unique<type>(val)) {}
    explicit Json(const jsonArray& val) : valuePtr_(std::make_unique<type>(val)) {}
    explicit Json(const jsonObject& val) : valuePtr_(std::make_unique<type>(val)) {}
    /* 委托构造函数 */
    explicit Json() : Json(nullptr) {}
    explicit Json(int val) : Json(1.0 * val) {}
    explicit Json(const char* cstr) : Json(std::string(cstr)) {}

    /* 拷贝构造函数 */
    Json(const Json&);
    /* 移动构造函数 */
    Json(Json&&);
    /* 赋值运算符重载，值传递，传实参给形参时：左值拷贝，右值移动 */
    Json& operator=(Json);

    ~Json() = default;

public:
    std::string serialize() const;

    JsonType getType() const;

    bool isNull() const;
    bool isBool() const;
    bool isNumber() const;
    bool isString() const;
    bool isArray() const;
    bool isObject() const;

    std::nullptr_t     toNull() const;
    bool               toBool() const;
    double             toNumber() const;
    const std::string& toString() const;
    const jsonArray&   toArray() const;
    const jsonObject&  toObject() const;

    /* 仅限 Array 和 Object 类型调用，只读*/
    std::size_t size() const;
    const Json& operator[](std::size_t) const;
    const Json& operator[](const std::string&) const;

private:
    std::string serializeString() const;
    std::string serializeArray() const;
    std::string serializeObject() const;
};

std::ostream& operator<<(std::ostream& os, const Json& json);

bool operator==(const Json& lhs, const Json& rhs);
bool operator!=(const Json& lhs, const Json& rhs);

}  // namespace lightJson

#endif