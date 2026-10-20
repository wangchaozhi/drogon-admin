#pragma once

#include "Validation.h"
#include <json/json.h>
#include <optional>
#include <string>

namespace modules::user::dto {

struct ChangePasswordReq {
    std::string currentPassword;
    std::string newPassword;

    static std::optional<ChangePasswordReq> parse(
        const Json::Value& json,
        std::string& error) {
        if (!json.isObject()) {
            error = "body must be json object";
            return std::nullopt;
        }
        if (!json.isMember("current_password")
            || !json["current_password"].isString()) {
            error = "current_password is required";
            return std::nullopt;
        }
        if (!json.isMember("new_password")
            || !json["new_password"].isString()) {
            error = "new_password is required";
            return std::nullopt;
        }

        ChangePasswordReq request{
            json["current_password"].asString(),
            json["new_password"].asString(),
        };
        if (request.currentPassword.empty()
            || request.currentPassword.size() > 256) {
            error = "current_password is invalid";
            return std::nullopt;
        }
        if (!validation::validPassword(request.newPassword)) {
            error = "new_password must be between 12 and 256 chars";
            return std::nullopt;
        }
        if (request.currentPassword == request.newPassword) {
            error = "new_password must differ from current_password";
            return std::nullopt;
        }
        return request;
    }
};

} // namespace modules::user::dto
