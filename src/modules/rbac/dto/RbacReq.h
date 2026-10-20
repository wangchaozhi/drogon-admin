#pragma once
//
// RBAC 请求 DTO：角色 / 菜单 / 分配类。
//
#include <json/json.h>
#include <algorithm>
#include <cctype>
#include <limits>
#include <optional>
#include <string>
#include <vector>
#include <cstdint>

namespace modules::rbac::dto {

inline std::string trimText(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

inline bool validRoleCode(const std::string& code) {
    if (code.empty() || code.size() > 64
        || !std::islower(static_cast<unsigned char>(code.front()))) {
        return false;
    }
    return std::all_of(
        code.begin(), code.end(),
        [](unsigned char character) {
            return std::islower(character)
                || std::isdigit(character)
                || character == '_'
                || character == '-'
                || character == ':';
        });
}

struct RoleUpsertReq {
    std::string code;
    std::string name;
    std::string description;

    // create=true 时必须校验 code；update 时 code 可缺省
    static std::optional<RoleUpsertReq> parse(const Json::Value& j, bool create, std::string& err) {
        if (!j.isObject()) { err = "body must be json object"; return std::nullopt; }
        if (create && (!j.isMember("code") || !j["code"].isString())) {
            err = "code must be a string";
            return std::nullopt;
        }
        if (!j.isMember("name") || !j["name"].isString()) {
            err = "name must be a string";
            return std::nullopt;
        }
        if (j.isMember("description") && !j["description"].isString()) {
            err = "description must be a string";
            return std::nullopt;
        }
        RoleUpsertReq r;
        r.code        = trimText(j.get("code", "").asString());
        r.name        = trimText(j["name"].asString());
        r.description = trimText(j.get("description", "").asString());
        if (create && !validRoleCode(r.code)) {
            err = "code must start with a lowercase letter and contain only lowercase letters, digits, _, -, or :";
            return std::nullopt;
        }
        if (r.name.empty())           { err = "name is required"; return std::nullopt; }
        if (r.name.size() > 100)      { err = "name must be at most 100 chars"; return std::nullopt; }
        if (r.description.size() > 500) { err = "description must be at most 500 chars"; return std::nullopt; }
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
        if (arr.size() > 500) { err = field + " must contain at most 500 ids"; return std::nullopt; }
        IdListReq r;
        r.ids.reserve(arr.size());
        for (const auto& v : arr) {
            if ((!v.isInt64() && !v.isUInt64())
                || (v.isUInt64()
                    && v.asUInt64()
                        > static_cast<Json::UInt64>(
                            std::numeric_limits<int64_t>::max()))) {
                err = field + " entries must be integers";
                return std::nullopt;
            }
            const auto id = v.asInt64();
            if (id <= 0) {
                err = field + " entries must be positive";
                return std::nullopt;
            }
            r.ids.push_back(id);
        }
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
        if (j.isMember("parent_id")
            && !j["parent_id"].isInt64()
            && !j["parent_id"].isUInt64()) {
            err = "parent_id must be an integer";
            return std::nullopt;
        }
        if (j.isMember("parent_id")
            && j["parent_id"].isUInt64()
            && j["parent_id"].asUInt64()
                > static_cast<Json::UInt64>(
                    std::numeric_limits<int64_t>::max())) {
            err = "parent_id is out of range";
            return std::nullopt;
        }
        if (!j.isMember("name") || !j["name"].isString()) {
            err = "name must be a string";
            return std::nullopt;
        }
        for (const auto* field : {"path", "icon", "component", "type"}) {
            if (j.isMember(field) && !j[field].isString()) {
                err = std::string(field) + " must be a string";
                return std::nullopt;
            }
        }
        if (j.isMember("sort") && !j["sort"].isIntegral()) {
            err = "sort must be an integer";
            return std::nullopt;
        }
        if (j.isMember("visible") && !j["visible"].isIntegral()) {
            err = "visible must be an integer";
            return std::nullopt;
        }
        MenuUpsertReq m;
        m.parentId  = j.get("parent_id", 0).asInt64();
        m.name      = trimText(j["name"].asString());
        m.path      = trimText(j.get("path", "").asString());
        m.icon      = trimText(j.get("icon", "").asString());
        m.component = trimText(j.get("component", "").asString());
        const auto sort = j.get("sort", 0).asInt64();
        const auto visible = j.get("visible", 1).asInt64();
        m.type      = j.get("type", "menu").asString();
        if (m.name.empty())  { err = "name is required"; return std::nullopt; }
        if (m.name.size() > 100) { err = "name must be at most 100 chars"; return std::nullopt; }
        if (m.parentId < 0) { err = "parent_id must not be negative"; return std::nullopt; }
        if (sort < -1000000 || sort > 1000000) { err = "sort is out of range"; return std::nullopt; }
        if (m.type != "dir" && m.type != "menu") { err = "type must be dir|menu"; return std::nullopt; }
        if (m.path.size() > 255) { err = "path must be at most 255 chars"; return std::nullopt; }
        if (m.icon.size() > 100) { err = "icon must be at most 100 chars"; return std::nullopt; }
        if (m.component.size() > 100) { err = "component must be at most 100 chars"; return std::nullopt; }
        if (visible != 0 && visible != 1) { err = "visible must be 0 or 1"; return std::nullopt; }
        m.sort = static_cast<int>(sort);
        m.visible = static_cast<int>(visible);
        return m;
    }
};

} // namespace modules::rbac::dto
