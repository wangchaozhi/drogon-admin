#pragma once
//
// 菜单实体 / 返回 DTO。支持树形子节点。
//
#include <json/json.h>
#include <string>
#include <vector>
#include <cstdint>

namespace modules::rbac::dto {

struct MenuDto {
    int64_t     id{0};
    int64_t     parentId{0};
    std::string name;
    std::string path;
    std::string icon;
    std::string component;
    int         sort{0};
    std::string type{"menu"};  // dir | menu
    int         visible{1};
    int64_t     createdAt{0};
    std::vector<MenuDto> children;

    Json::Value toJson() const {
        Json::Value j;
        j["id"]         = static_cast<Json::Int64>(id);
        j["parent_id"]  = static_cast<Json::Int64>(parentId);
        j["name"]       = name;
        j["path"]       = path;
        j["icon"]       = icon;
        j["component"]  = component;
        j["sort"]       = sort;
        j["type"]       = type;
        j["visible"]    = visible;
        j["created_at"] = static_cast<Json::Int64>(createdAt);
        if (!children.empty()) {
            Json::Value arr(Json::arrayValue);
            for (const auto& c : children) arr.append(c.toJson());
            j["children"] = arr;
        }
        return j;
    }
};

} // namespace modules::rbac::dto
