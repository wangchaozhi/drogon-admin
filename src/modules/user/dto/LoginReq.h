#pragma once
//
// 登录请求 DTO：邮箱 + 密码。
//
#include <json/json.h>
#include <optional>
#include <string>

namespace modules::user::dto {

struct LoginReq {
    std::string email;
    std::string password;

    static std::optional<LoginReq> parse(const Json::Value& j,
                                         std::string& err) {
        if (!j.isObject()) {
            err = "body must be json object";
            return std::nullopt;
        }
        LoginReq req;
        req.email    = j.get("email", "").asString();
        req.password = j.get("password", "").asString();
        if (req.email.empty())    { err = "email is required";    return std::nullopt; }
        if (req.password.empty()) { err = "password is required"; return std::nullopt; }
        return req;
    }
};

} // namespace modules::user::dto
