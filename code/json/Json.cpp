#include "Json.h"

namespace lightJson {

/**
 * @description: 拷贝构造函数 
 *  因为是独占智能指针管理Json对象实例，所以需重新构造新的Json对象
 * @param {Json&} rhs
 * @return {Json}
 */
Json::Json(const Json& rhs) {
    switch (rhs.getType()) {
        case JsonType::Null: {
            valuePtr_ = std::make_unique<type>(nullptr);
            break;
        }
        case JsonType::Bool: {
            valuePtr_ = std::make_unique<type>(rhs.toBool());
            break;
        }
        case JsonType::Number: {
            valuePtr_ = std::make_unique<type>(rhs.toNumber());
            break;
        }
        case JsonType::String: {
            valuePtr_ = std::make_unique<type>(rhs.toString());
            break;
        }
        case JsonType::Array: {
            valuePtr_ = std::make_unique<type>(rhs.toArray());
            break;
        }
        case JsonType::Object: {
            valuePtr_ = std::make_unique<type>(rhs.toObject());
            break;
        }
        default: {
            break;
        }
    }
}

Json::Json(Json&& rhs) : valuePtr_(std::move(rhs.valuePtr_)) { rhs.valuePtr_ = nullptr; }

Json& Json::operator=(Json rhs) {
    Json temp(rhs);
    std::swap(valuePtr_, temp.valuePtr_);
    return *this;
}

/**
 * @description: 将json对象，转为 字符串
 * @return {*}
 */
std::string Json::serialize() const {
    switch (getType()) {
        case JsonType::Null:
            return "null";
        case JsonType::Bool:
            return toBool() ? "true" : "false";
        case JsonType::Number:
            char buf[32];
            sprintf(buf, "%.17g", toNumber());
            return std::string(buf);
        case JsonType::String:
            return serializeString();
        case JsonType::Array:
            return serializeArray();
        default:
            return serializeObject();
    }
}

std::string Json::serializeString() const {
    std::string res = "\"";
    for (auto c : toString()) {
        switch (c) {
            case '\"':
                res += "\\\"";
                break;
            case '\\':
                res += "\\\\";
                break;
            case '\b':
                res += "\\b";
                break;
            case '\f':
                res += "\\f";
                break;
            case '\n':
                res += "\\n";
                break;
            case '\r':
                res += "\\r";
                break;
            case '\t':
                res += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[7];
                    sprintf(buf, "\\u%04X", c);
                    res += buf;
                } else {
                    res += c;
                }
        }
    }
    return res + '\"';
}

std::string Json::serializeArray() const {
    std::string res = "[ ";
    for (std::size_t i = 0; i != size(); ++i) {
        if (i > 0) {
            res += ", ";
        }
        res += operator[](i).serialize();
    }
    return res + " ]";
}

std::string Json::serializeObject() const {
    std::string res     = "{ ";
    bool        isFirst = true;
    for (auto& p : toObject()) {
        if (isFirst) {
            isFirst = false;
        } else {
            res += ", ";
        }
        res += "\"" + p.first + "\"";
        res += ": ";
        res += p.second.serialize();
    }
    return res + " }";
}

JsonType Json::getType() const {
    if (std::holds_alternative<std::nullptr_t>(getVariant())) {
        return JsonType::Null;
    } else if (std::holds_alternative<bool>(getVariant())) {
        return JsonType::Bool;
    } else if (std::holds_alternative<double>(getVariant())) {
        return JsonType::Number;
    } else if (std::holds_alternative<std::string>(getVariant())) {
        return JsonType::String;
    } else if (std::holds_alternative<jsonArray>(getVariant())) {
        return JsonType::Array;
    } else {
        return JsonType::Object;
    }
}

bool Json::isNull() const { return getType() == JsonType::Null; }
bool Json::isBool() const { return getType() == JsonType::Bool; }
bool Json::isNumber() const { return getType() == JsonType::Number; }
bool Json::isString() const { return getType() == JsonType::String; }
bool Json::isArray() const { return getType() == JsonType::Array; }
bool Json::isObject() const { return getType() == JsonType::Object; }

std::nullptr_t Json::toNull() const {
    if (isNull()) {
        return std::get<std::nullptr_t>(getVariant());
    } else {
        throw JsonException("not a null");
    }
}

bool Json::toBool() const {
    if (isBool()) {
        return std::get<bool>(getVariant());
    } else {
        throw JsonException("not a bool");
    }
}

double Json::toNumber() const {
    if (isNumber()) {
        return std::get<double>(getVariant());
    } else {
        throw JsonException("not a number");
    }
}

const std::string& Json::toString() const {
    if (isString()) {
        return std::get<std::string>(getVariant());
    } else {
        throw JsonException("not a string");
    }
}

/**
 * @description: 如果该json对象是 jsonArray 类型，就返回 jsonArray 类型的对象
 * @return {JsonArray}
 */
const jsonArray& Json::toArray() const {
    if (isArray()) {
        return std::get<jsonArray>(getVariant());
    } else {
        throw JsonException("not a jsonArray");
    }
}

/**
 * @description: 如果该json对象是 jsonObject 类型，就返回 jsonObject 类型的对象
 * @return {JsonObject}
 */
const jsonObject& Json::toObject() const {
    if (isObject()) {
        return std::get<jsonObject>(getVariant());
    } else {
        throw JsonException("not a jsonObject");
    }
}

std::size_t Json::size() const {
    if (isArray()) {
        return std::get<jsonArray>(getVariant()).size();
    } else if (isObject()) {
        return std::get<jsonObject>(getVariant()).size();
    } else {
        throw JsonException("not a jsonArray or jsonObject");
    }
}

/**
 * @description: 重载 [] 运算符，返回json对象，由 JsonArray 数组类型调用
 * @return {Json}
 */
const Json& Json::operator[](size_t pos) const {
    if (isArray()) {
        return std::get<jsonArray>(getVariant()).at(pos);
    } else {
        throw JsonException("not a jsonArray");
    }
}

/**
 * @description: 重载 [] 运算符，返回json对象，由 JsonObject 类型调用
 * @return {Json}
 */
const Json& Json::operator[](const std::string& key) const {
    if (isObject()) {
        return std::get<jsonObject>(getVariant()).at(key);
    } else {
        throw JsonException("not a jsonObject");
    }
}

std::ostream& operator<<(std::ostream& os, const Json& json) { return os << json.serialize(); }

bool operator==(const Json& lhs, const Json& rhs) {
    if (lhs.getType() != rhs.getType()) {
        return false;
    }
    switch (lhs.getType()) {
        case JsonType::Null: {
            return true;
        }
        case JsonType::Bool: {
            return lhs.toBool() == rhs.toBool();
        }
        case JsonType::Number: {
            return lhs.toNumber() == rhs.toNumber();
        }
        case JsonType::String: {
            return lhs.toString() == rhs.toString();
        }
        case JsonType::Array: {
            return lhs.toArray() == rhs.toArray();
        }
        case JsonType::Object: {
            return lhs.toObject() == rhs.toObject();
        }
        default: {
            return false;
        }
    }
}

bool operator!=(const Json& lhs, const Json& rhs) { return !(lhs == rhs); }

/**
 * @description: 对外接口，调用解析函数
 * @param {string&} content
 * @param {string&} errMsg
 * @return {Json}
 */
Json JSON_API(const std::string& content, std::string& errMsg) {
    try {
        JsonParser p(content);
        return p.parseJson();
    } catch (JsonException& e) {
        errMsg = e.what();
        return Json(nullptr);
    }
}

}  // namespace lightJson
