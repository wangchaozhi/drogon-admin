#pragma once
//
// 更新用户请求 DTO：昵称与邮箱必填，密码可选（留空表示不修改）。
//
#include <json/json.h>
#include <optional>
#include <string>

namespace modules::user::dto {

struct UpdateUserReq {
    std::string name;
    std::string email;
    std::optional<std::string> password;  // 可选：仅在显式传入时才修改

    static std::optional<UpdateUserReq> parse(const Json::Value& j,
                                              std::string& err) {
        if (!j.isObject()) {
            err = "body must be json object";
            return std::nullopt;
        }
        UpdateUserReq req;
        req.name  = j.get("name",  "").asString();
        req.email = j.get("email", "").asString();
        if (req.name.empty())  { err = "name is required";  return std::nullopt; }
        if (req.email.empty()) { err = "email is required"; return std::nullopt; }
        if (j.isMember("password")) {
            auto pwd = j["password"].asString();
            if (!pwd.empty()) {
                if (pwd.size() < 6) {
                    err = "password must be at least 6 chars";
                    return std::nullopt;
                }
                req.password = std::move(pwd);
            }
        }
        return req;
    }
};

} // namespace modules::user::dto
