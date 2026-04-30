#pragma once
//
// 创建用户请求 DTO（同时用于注册）+ 简单校验。
//
#include <json/json.h>
#include <optional>
#include <string>

namespace modules::user::dto {

struct CreateUserReq {
    std::string name;
    std::string email;
    std::string password;

    // 解析 JSON，失败时返回错误描述
    static std::optional<CreateUserReq> parse(const Json::Value& j,
                                              std::string& err) {
        if (!j.isObject()) {
            err = "body must be json object";
            return std::nullopt;
        }
        CreateUserReq req;
        req.name     = j.get("name", "").asString();
        req.email    = j.get("email", "").asString();
        req.password = j.get("password", "").asString();
        if (req.name.empty())      { err = "name is required";     return std::nullopt; }
        if (req.email.empty())     { err = "email is required";    return std::nullopt; }
        if (req.password.size() < 6) {
            err = "password must be at least 6 chars";
            return std::nullopt;
        }
        return req;
    }
};

} // namespace modules::user::dto
