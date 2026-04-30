#pragma once
//
// 用户实体 / 返回 DTO。业务内存模型，与数据库模型解耦。
//
#include <json/json.h>
#include <string>
#include <cstdint>

namespace modules::user::dto {

struct UserDto {
    int64_t     id{0};
    std::string name;
    std::string email;
    int64_t     createdAt{0};

    Json::Value toJson() const {
        Json::Value j;
        j["id"]         = static_cast<Json::Int64>(id);
        j["name"]       = name;
        j["email"]      = email;
        j["created_at"] = static_cast<Json::Int64>(createdAt);
        return j;
    }
};

} // namespace modules::user::dto
