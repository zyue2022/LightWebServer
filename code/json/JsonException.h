/*
 * @Description  : 保存异常信息的类，用于逻辑出现问题时
 * @Date         : 2022-07-19 14:48:31
 * @LastEditTime : 2022-07-20 19:14:38
 */
#ifndef JSON_EXCEP_H
#define JSON_EXCEP_H

#include <stdexcept>
#include <string>

namespace lightJson {

class JsonException final : public std::logic_error {
public:
    explicit JsonException(const std::string& errMsg) : logic_error(errMsg) {}
};

}  // namespace lightJson

#endif