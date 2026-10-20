#pragma once
//
// 登录请求 DTO：邮箱 + 密码。
//
#include "Validation.h"
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
        if (!j.isMember("email") || !j["email"].isString()) {
            err = "email must be a string";
            return std::nullopt;
        }
        if (!j.isMember("password") || !j["password"].isString()) {
            err = "password must be a string";
            return std::nullopt;
        }
        LoginReq req;
        req.email    = validation::normalizeEmail(j["email"].asString());
        req.password = j["password"].asString();
        if (!validation::validEmail(req.email)) {
            err = "email format is invalid";
            return std::nullopt;
        }
        if (req.password.empty() || req.password.size() > 256) {
            err = "password must be between 1 and 256 chars";
            return std::nullopt;
        }
        return req;
    }
};

} // namespace modules::user::dto
