#pragma once
//
// 更新用户请求 DTO：昵称与邮箱必填，密码可选（留空表示不修改）。
//
#include "Validation.h"
#include <json/json.h>
#include <optional>
#include <string>

namespace modules::user::dto {

struct UpdateUserReq {
    std::string name;
    std::string email;
    std::optional<std::string> password;  // 可选：仅在显式传入时才修改
    std::optional<bool> disabled;          // 可选：管理员启用/禁用账号

    static std::optional<UpdateUserReq> parse(const Json::Value& j,
                                              std::string& err) {
        if (!j.isObject()) {
            err = "body must be json object";
            return std::nullopt;
        }
        UpdateUserReq req;
        req.name  = j.get("name",  "").asString();
        req.email = j.get("email", "").asString();
        if (req.name.empty()) { err = "name is required"; return std::nullopt; }
        if (req.name.size() > 100) {
            err = "name must be at most 100 chars";
            return std::nullopt;
        }
        if (!validation::validEmail(req.email)) {
            err = "email format is invalid";
            return std::nullopt;
        }
        if (j.isMember("password")) {
            if (!j["password"].isString()) {
                err = "password must be a string";
                return std::nullopt;
            }
            auto pwd = j["password"].asString();
            if (!pwd.empty()) {
                if (!validation::validPassword(pwd)) {
                    err = "password must be between 12 and 256 chars";
                    return std::nullopt;
                }
                req.password = std::move(pwd);
            }
        }
        if (j.isMember("disabled")) {
            if (!j["disabled"].isBool()) {
                err = "disabled must be boolean";
                return std::nullopt;
            }
            req.disabled = j["disabled"].asBool();
        }
        return req;
    }
};

} // namespace modules::user::dto
