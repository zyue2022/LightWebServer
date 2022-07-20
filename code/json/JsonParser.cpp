#include "JsonParser.h"

#include "Json.h"

namespace lightJson {

bool is1to9(char ch) { return ch != '0' && isdigit(ch); }
bool is0to9(char ch) { return isdigit(ch); }

/**
 * @description: 解析布尔类型 或 null
 */
Json JsonParser::parseLiteral(const std::string& literal) {
    if (strncmp(_curr, literal.c_str(), literal.size()) != 0) {
        throw JsonException("parse invalid value");
    }
    _curr += literal.size();
    _start = _curr;
    switch (literal[0]) {
        case 't':
            return Json(true);
        case 'f':
            return Json(false);
        default:
            return Json(nullptr);
    }
}

/**
 * @description: 解析数值类型，一般取浮点数类型
 *  注意检测数值合法性 ：
 *          - 小数点前
 *          - 小数点后
 *          - 指数部分
 */
Json JsonParser::parseNumber() {
    if (*_curr == '-') {
        ++_curr;
    }
    if (*_curr == '0') {
        ++_curr;
    } else {
        if (!is1to9(*_curr)) {
            throw JsonException("parse invalid value");
        }
        while (is0to9(*++_curr)) {
        }
    }
    if (*_curr == '.') {
        if (!is0to9(*++_curr)) {
            throw JsonException("parse invalid value");
        }
        while (is0to9(*++_curr)) {
        }
    }
    if (*_curr == 'E' || *_curr == 'e') {
        ++_curr;
        if (*_curr == '-' || *_curr == '+') {
            ++_curr;
        }
        if (!is0to9(*_curr)) {
            throw JsonException("parse invalid value");
        }
        while (is0to9(*++_curr)) {
        }
    }
    // 把参数 str 所指向的字符串转换为一个浮点数（ double 型）
    double val = strtod(_start, nullptr);
    if (fabs(val) == HUGE_VAL) {
        throw JsonException("parse number too big");
    }
    _start = _curr;
    return Json(val);
}

unsigned JsonParser::parse4hex() {
    unsigned u = 0;
    for (int i = 0; i != 4; ++i) {
        char ch = *++_curr;
        u <<= 4;
        if (ch >= '0' && ch <= '9') {
            u |= ch - '0';
        } else if (ch >= 'A' && ch <= 'F') {
            u |= ch - ('A' - 10);
        } else if (ch >= 'a' && ch <= 'f') {
            u |= ch - ('a' - 10);
        } else {
            throw JsonException("parse invalid unicode hex");
        }
    }
    return u;
}

std::string JsonParser::encodeUTF8(unsigned u) {
    std::string utf8;
    if (u <= 0x7F) {
        utf8.push_back(static_cast<char>(u & 0xff));
    } else if (u <= 0x7FF) {
        utf8.push_back(static_cast<char>(0xc0 | ((u >> 6) & 0xff)));
        utf8.push_back(static_cast<char>(0x80 | (u & 0x3f)));
    } else if (u <= 0xFFFF) {
        utf8.push_back(static_cast<char>(0xe0 | ((u >> 12) & 0xff)));
        utf8.push_back(static_cast<char>(0x80 | ((u >> 6) & 0x3f)));
        utf8.push_back(static_cast<char>(0x80 | (u & 0x3f)));
    } else {
        assert(u <= 0x10FFFF);
        utf8.push_back(static_cast<char>(0xf0 | ((u >> 18) & 0xff)));
        utf8.push_back(static_cast<char>(0x80 | ((u >> 12) & 0x3f)));
        utf8.push_back(static_cast<char>(0x80 | ((u >> 6) & 0x3f)));
        utf8.push_back(static_cast<char>(0x80 | (u & 0x3f)));
    }
    return utf8;
}

/**
 * @description: 解析然后返回原始字符串
 *  先处理转义字符，再检测合法性
 */
std::string JsonParser::parseRawString() {
    std::string str;
    while (true) {
        switch (*++_curr) {
            case '\"':
                _start = ++_curr;
                return str;
            case '\0':
                throw JsonException("parse miss quotation mark");
            case '\\':
                switch (*++_curr) {
                    case '\"':
                        str.push_back('\"');
                        break;
                    case '\\':
                        str.push_back('\\');
                        break;
                    case '/':
                        str.push_back('/');
                        break;
                    case 'b':
                        str.push_back('\b');
                        break;
                    case 'f':
                        str.push_back('\f');
                        break;
                    case 'n':
                        str.push_back('\n');
                        break;
                    case 't':
                        str.push_back('\t');
                        break;
                    case 'r':
                        str.push_back('\r');
                        break;
                    case 'u': {
                        unsigned u1 = parse4hex();
                        if (u1 >= 0xd800 && u1 <= 0xdbff) {
                            if (*++_curr != '\\') {
                                throw JsonException("parse invalid unicode surrogate");
                            }
                            if (*++_curr != 'u') {
                                throw JsonException("parse invalid unicode surrogate");
                            }
                            unsigned u2 = parse4hex();
                            if (u2 < 0xdc00 || u2 > 0xdfff) {
                                throw(JsonException("parse invalid unicode surrogate"));
                            }
                            u1 = (((u1 - 0xd800) << 10) | (u2 - 0xdc00)) + 0x10000;
                        }
                        str += encodeUTF8(u1);
                    } break;
                    default:
                        throw(JsonException("parse invalid string escape"));
                }
                break;
            default:
                if (static_cast<unsigned char>(*_curr) < 0x20) {
                    throw(JsonException("parse invalid string char"));
                }
                str.push_back(*_curr);
                break;
        }
    }
}

/**
 * @description: 解析字符串
 * @return {Json}
 */
Json JsonParser::parseString() { return Json(parseRawString()); }

/**
 * @description: 解析数组类型，递归处理
 * @return {Json}
 */
Json JsonParser::parseArray() {
    jsonArray arr;
    ++_curr;
    jumpSpace();
    if (*_curr == ']') {
        _start = ++_curr;
        return Json(arr);
    }
    while (true) {
        jumpSpace();
        arr.push_back(parseValue());
        jumpSpace();
        if (*_curr == ',') {
            ++_curr;
        } else if (*_curr == ']') {
            _start = ++_curr;
            return (Json(arr));
        } else {
            throw(JsonException("parse miss comma or square bracket"));
        }
    }
}

/**
 * @description: 解析对象类型，递归处理
 * @return {Json}
 */
Json JsonParser::parseObject() {
    jsonObject obj;
    ++_curr;
    jumpSpace();
    if (*_curr == '}') {
        _start = ++_curr;
        return Json(obj);
    }
    while (true) {
        jumpSpace();
        if (*_curr != '\"') {
            throw(JsonException("parse miss key"));
        }
        std::string key = parseRawString();
        jumpSpace();
        if (*_curr++ != ':') {
            throw(JsonException("parse miss colon"));
        }
        jumpSpace();
        Json val = parseValue();
        obj.emplace(key, val);
        jumpSpace();
        if (*_curr == ',') {
            ++_curr;
        } else if (*_curr == '}') {
            _start = ++_curr;
            return Json(obj);
        } else {
            throw JsonException("parse miss comma or curly bracket");
        }
    }
}

/**
 * @description: 解析空白处，即跳过空白
 * 空白可能有：空格、Tab、换行、回车
 */
void JsonParser::jumpSpace() {
    while (*_curr == ' ' || *_curr == '\t' || *_curr == '\n' || *_curr == '\r') {
        ++_curr;
    }
    _start = _curr;
}

/**
 * @description: 解析Json中键值对的 值
 */
Json JsonParser::parseValue() {
    switch (*_curr) {
        case 'n':
            return parseLiteral("null");
        case 't':
            return parseLiteral("true");
        case 'f':
            return parseLiteral("false");
        case '\"':
            return parseString();
        case '[':
            return parseArray();
        case '{':
            return parseObject();
        case '\0':
            throw JsonException("parse expect value");
        default:
            return parseNumber();
    }
}

/**
 * @description: 解析
 * 过程就是：跳过空白、解析Json、跳过空白
 */
Json JsonParser::parseJson() {
    jumpSpace();
    Json json = parseValue();
    jumpSpace();
    if (*_curr) {
        throw(JsonException("parse root not singular"));
    }
    return json;
}

}  // namespace lightJson
