#pragma once
//
// 角色实体 / 返回 DTO。
//
#include <json/json.h>
#include <string>
#include <cstdint>

namespace modules::rbac::dto {

struct RoleDto {
    int64_t     id{0};
    std::string code;
    std::string name;
    std::string description;
    int64_t     createdAt{0};

    Json::Value toJson() const {
        Json::Value j;
        j["id"]          = static_cast<Json::Int64>(id);
        j["code"]        = code;
        j["name"]        = name;
        j["description"] = description;
        j["created_at"]  = static_cast<Json::Int64>(createdAt);
        return j;
    }
};

} // namespace modules::rbac::dto
