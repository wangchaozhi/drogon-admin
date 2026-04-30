#pragma once
//
// 权限实体 / 返回 DTO。
//
#include <json/json.h>
#include <string>
#include <cstdint>

namespace modules::rbac::dto {

struct PermissionDto {
    int64_t     id{0};
    std::string code;
    std::string name;
    std::string type{"button"};  // menu | button
    int64_t     menuId{0};
    int64_t     createdAt{0};

    Json::Value toJson() const {
        Json::Value j;
        j["id"]         = static_cast<Json::Int64>(id);
        j["code"]       = code;
        j["name"]       = name;
        j["type"]       = type;
        j["menu_id"]    = static_cast<Json::Int64>(menuId);
        j["created_at"] = static_cast<Json::Int64>(createdAt);
        return j;
    }
};

} // namespace modules::rbac::dto
