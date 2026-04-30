#pragma once
//
// RBAC 请求 DTO：角色 / 菜单 / 分配类。
//
#include <json/json.h>
#include <optional>
#include <string>
#include <vector>
#include <cstdint>

namespace modules::rbac::dto {

struct RoleUpsertReq {
    std::string code;
    std::string name;
    std::string description;

    // create=true 时必须校验 code；update 时 code 可缺省
    static std::optional<RoleUpsertReq> parse(const Json::Value& j, bool create, std::string& err) {
        if (!j.isObject()) { err = "body must be json object"; return std::nullopt; }
        RoleUpsertReq r;
        r.code        = j.get("code", "").asString();
        r.name        = j.get("name", "").asString();
        r.description = j.get("description", "").asString();
        if (create && r.code.empty()) { err = "code is required"; return std::nullopt; }
        if (r.name.empty())           { err = "name is required"; return std::nullopt; }
        return r;
    }
};

struct IdListReq {
    std::vector<int64_t> ids;

    // 支持 {"ids":[1,2,3]} 或 {"permission_ids":[...]}/{"role_ids":[...]}
    static std::optional<IdListReq> parse(const Json::Value& j, const std::string& field, std::string& err) {
        if (!j.isObject()) { err = "body must be json object"; return std::nullopt; }
        const Json::Value& arr = j.isMember(field) ? j[field] : j["ids"];
        if (!arr.isArray()) { err = field + " must be array"; return std::nullopt; }
        IdListReq r;
        r.ids.reserve(arr.size());
        for (const auto& v : arr) r.ids.push_back(v.asInt64());
        return r;
    }
};

struct MenuUpsertReq {
    int64_t     parentId{0};
    std::string name;
    std::string path;
    std::string icon;
    std::string component;
    int         sort{0};
    std::string type{"menu"};
    int         visible{1};

    static std::optional<MenuUpsertReq> parse(const Json::Value& j, std::string& err) {
        if (!j.isObject()) { err = "body must be json object"; return std::nullopt; }
        MenuUpsertReq m;
        m.parentId  = j.get("parent_id", 0).asInt64();
        m.name      = j.get("name", "").asString();
        m.path      = j.get("path", "").asString();
        m.icon      = j.get("icon", "").asString();
        m.component = j.get("component", "").asString();
        m.sort      = j.get("sort", 0).asInt();
        m.type      = j.get("type", "menu").asString();
        m.visible   = j.get("visible", 1).asInt();
        if (m.name.empty())  { err = "name is required"; return std::nullopt; }
        if (m.type != "dir" && m.type != "menu") { err = "type must be dir|menu"; return std::nullopt; }
        return m;
    }
};

} // namespace modules::rbac::dto
