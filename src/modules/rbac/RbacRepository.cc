#include "RbacRepository.h"
#include "common/TimeUtil.h"
#include "core/Logger.h"
#include <drogon/drogon.h>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <functional>

namespace modules::rbac {

drogon::orm::DbClientPtr RbacRepository::db() const {
    return drogon::app().getDbClient();
}

// ==================== 角色 ====================

drogon::Task<std::vector<dto::RoleDto>> RbacRepository::listRoles() {
    std::vector<dto::RoleDto> out;
    auto r = co_await db()->execSqlCoro(
        "SELECT id,code,name,description,created_at FROM roles ORDER BY id ASC");
    out.reserve(r.size());
    for (const auto& row : r) {
        dto::RoleDto d;
        d.id          = row["id"].as<int64_t>();
        d.code        = row["code"].as<std::string>();
        d.name        = row["name"].as<std::string>();
        d.description = row["description"].as<std::string>();
        d.createdAt   = row["created_at"].as<int64_t>();
        out.push_back(std::move(d));
    }
    co_return out;
}

drogon::Task<std::optional<dto::RoleDto>> RbacRepository::getRole(int64_t id) {
    auto r = co_await db()->execSqlCoro(
        "SELECT id,code,name,description,created_at FROM roles WHERE id=?", id);
    if (r.empty()) co_return std::nullopt;
    const auto& row = r[0];
    dto::RoleDto d;
    d.id          = row["id"].as<int64_t>();
    d.code        = row["code"].as<std::string>();
    d.name        = row["name"].as<std::string>();
    d.description = row["description"].as<std::string>();
    d.createdAt   = row["created_at"].as<int64_t>();
    co_return d;
}

drogon::Task<std::optional<dto::RoleDto>>
RbacRepository::getRoleByCode(std::string code) {
    auto r = co_await db()->execSqlCoro(
        "SELECT id,code,name,description,created_at FROM roles WHERE code=?", code);
    if (r.empty()) co_return std::nullopt;
    const auto& row = r[0];
    dto::RoleDto d;
    d.id          = row["id"].as<int64_t>();
    d.code        = row["code"].as<std::string>();
    d.name        = row["name"].as<std::string>();
    d.description = row["description"].as<std::string>();
    d.createdAt   = row["created_at"].as<int64_t>();
    co_return d;
}

drogon::Task<dto::RoleDto> RbacRepository::createRole(dto::RoleUpsertReq r) {
    int64_t now = common::TimeUtil::nowSec();
    auto res = co_await db()->execSqlCoro(
        "INSERT INTO roles(code,name,description,created_at) VALUES(?,?,?,?)",
        r.code, r.name, r.description, now);
    dto::RoleDto d;
    d.id          = res.insertId();
    d.code        = std::move(r.code);
    d.name        = std::move(r.name);
    d.description = std::move(r.description);
    d.createdAt   = now;
    co_return d;
}

drogon::Task<bool> RbacRepository::updateRole(int64_t id, dto::RoleUpsertReq r) {
    auto res = co_await db()->execSqlCoro(
        "UPDATE roles SET name=?, description=? WHERE id=?",
        r.name, r.description, id);
    co_return res.affectedRows() > 0;
}

drogon::Task<bool> RbacRepository::deleteRole(int64_t id) {
    auto res = co_await db()->execSqlCoro("DELETE FROM roles WHERE id=?", id);
    co_await db()->execSqlCoro("DELETE FROM role_permissions WHERE role_id=?", id);
    co_await db()->execSqlCoro("DELETE FROM user_roles WHERE role_id=?", id);
    co_return res.affectedRows() > 0;
}

drogon::Task<std::vector<int64_t>>
RbacRepository::getRolePermissionIds(int64_t roleId) {
    std::vector<int64_t> ids;
    auto r = co_await db()->execSqlCoro(
        "SELECT permission_id FROM role_permissions WHERE role_id=?", roleId);
    ids.reserve(r.size());
    for (const auto& row : r) ids.push_back(row["permission_id"].as<int64_t>());
    co_return ids;
}

drogon::Task<> RbacRepository::setRolePermissions(int64_t roleId,
                                                  std::vector<int64_t> permIds) {
    auto c = db();
    co_await c->execSqlCoro("DELETE FROM role_permissions WHERE role_id=?", roleId);
    for (auto pid : permIds) {
        co_await c->execSqlCoro(
            "INSERT OR IGNORE INTO role_permissions(role_id,permission_id) VALUES(?,?)",
            roleId, pid);
    }
    co_return;
}

// ==================== 菜单 ====================

namespace {
dto::MenuDto rowToMenu(const drogon::orm::Row& row) {
    dto::MenuDto m;
    m.id        = row["id"].as<int64_t>();
    m.parentId  = row["parent_id"].as<int64_t>();
    m.name      = row["name"].as<std::string>();
    m.path      = row["path"].as<std::string>();
    m.icon      = row["icon"].as<std::string>();
    m.component = row["component"].as<std::string>();
    m.sort      = row["sort"].as<int>();
    m.type      = row["type"].as<std::string>();
    m.visible   = row["visible"].as<int>();
    m.createdAt = row["created_at"].as<int64_t>();
    return m;
}
} // namespace

drogon::Task<std::vector<dto::MenuDto>> RbacRepository::listMenus() {
    std::vector<dto::MenuDto> out;
    auto r = co_await db()->execSqlCoro(
        "SELECT id,parent_id,name,path,icon,component,sort,type,visible,created_at "
        "FROM menus ORDER BY sort ASC, id ASC");
    out.reserve(r.size());
    for (const auto& row : r) out.push_back(rowToMenu(row));
    co_return out;
}

drogon::Task<std::vector<dto::MenuDto>> RbacRepository::menuTree() {
    auto flat = co_await listMenus();
    co_return buildTree(std::move(flat));
}

drogon::Task<std::optional<dto::MenuDto>> RbacRepository::getMenu(int64_t id) {
    auto r = co_await db()->execSqlCoro(
        "SELECT id,parent_id,name,path,icon,component,sort,type,visible,created_at "
        "FROM menus WHERE id=?", id);
    if (r.empty()) co_return std::nullopt;
    co_return rowToMenu(r[0]);
}

drogon::Task<dto::MenuDto> RbacRepository::createMenu(dto::MenuUpsertReq m) {
    int64_t now = common::TimeUtil::nowSec();
    auto res = co_await db()->execSqlCoro(
        "INSERT INTO menus(parent_id,name,path,icon,component,sort,type,visible,created_at) "
        "VALUES(?,?,?,?,?,?,?,?,?)",
        m.parentId, m.name, m.path, m.icon, m.component,
        m.sort, m.type, m.visible, now);
    dto::MenuDto d;
    d.id        = res.insertId();
    d.parentId  = m.parentId;
    d.name      = std::move(m.name);
    d.path      = std::move(m.path);
    d.icon      = std::move(m.icon);
    d.component = std::move(m.component);
    d.sort      = m.sort;
    d.type      = std::move(m.type);
    d.visible   = m.visible;
    d.createdAt = now;
    co_return d;
}

drogon::Task<bool> RbacRepository::updateMenu(int64_t id, dto::MenuUpsertReq m) {
    auto res = co_await db()->execSqlCoro(
        "UPDATE menus SET parent_id=?, name=?, path=?, icon=?, component=?, "
        "sort=?, type=?, visible=? WHERE id=?",
        m.parentId, m.name, m.path, m.icon, m.component,
        m.sort, m.type, m.visible, id);
    co_return res.affectedRows() > 0;
}

drogon::Task<bool> RbacRepository::deleteMenu(int64_t id) {
    auto c = db();
    // 顺便删除挂在该菜单下的权限
    co_await c->execSqlCoro("DELETE FROM permissions WHERE menu_id=?", id);
    auto res = co_await c->execSqlCoro("DELETE FROM menus WHERE id=?", id);
    co_return res.affectedRows() > 0;
}

// ==================== 权限 ====================

drogon::Task<std::vector<dto::PermissionDto>> RbacRepository::listPermissions() {
    std::vector<dto::PermissionDto> out;
    auto r = co_await db()->execSqlCoro(
        "SELECT id,code,name,type,menu_id,created_at FROM permissions "
        "ORDER BY menu_id ASC, id ASC");
    out.reserve(r.size());
    for (const auto& row : r) {
        dto::PermissionDto p;
        p.id        = row["id"].as<int64_t>();
        p.code      = row["code"].as<std::string>();
        p.name      = row["name"].as<std::string>();
        p.type      = row["type"].as<std::string>();
        p.menuId    = row["menu_id"].as<int64_t>();
        p.createdAt = row["created_at"].as<int64_t>();
        out.push_back(std::move(p));
    }
    co_return out;
}

// ==================== 用户关联 ====================

drogon::Task<std::vector<int64_t>>
RbacRepository::getUserRoleIds(int64_t userId) {
    std::vector<int64_t> ids;
    auto r = co_await db()->execSqlCoro(
        "SELECT role_id FROM user_roles WHERE user_id=?", userId);
    ids.reserve(r.size());
    for (const auto& row : r) ids.push_back(row["role_id"].as<int64_t>());
    co_return ids;
}

drogon::Task<> RbacRepository::setUserRoles(int64_t userId,
                                            std::vector<int64_t> roleIds) {
    auto c = db();
    co_await c->execSqlCoro("DELETE FROM user_roles WHERE user_id=?", userId);
    for (auto rid : roleIds) {
        co_await c->execSqlCoro(
            "INSERT OR IGNORE INTO user_roles(user_id,role_id) VALUES(?,?)",
            userId, rid);
    }
    co_return;
}

drogon::Task<std::vector<std::string>>
RbacRepository::getUserRoleCodes(int64_t userId) {
    std::vector<std::string> codes;
    auto r = co_await db()->execSqlCoro(
        "SELECT r.code FROM roles r JOIN user_roles ur ON ur.role_id = r.id "
        "WHERE ur.user_id = ? ORDER BY r.id",
        userId);
    codes.reserve(r.size());
    for (const auto& row : r) codes.push_back(row["code"].as<std::string>());
    co_return codes;
}

drogon::Task<std::vector<std::string>>
RbacRepository::getUserPermCodes(int64_t userId) {
    std::vector<std::string> codes;
    auto r = co_await db()->execSqlCoro(
        "SELECT DISTINCT p.code FROM permissions p "
        "JOIN role_permissions rp ON rp.permission_id = p.id "
        "JOIN user_roles ur ON ur.role_id = rp.role_id "
        "WHERE ur.user_id = ? ORDER BY p.code",
        userId);
    codes.reserve(r.size());
    for (const auto& row : r) codes.push_back(row["code"].as<std::string>());
    co_return codes;
}

drogon::Task<std::vector<dto::MenuDto>>
RbacRepository::getUserMenuTree(int64_t userId) {
    // 1. 用户有权限进入的菜单 id（permissions.menu_id）
    auto r = co_await db()->execSqlCoro(
        "SELECT DISTINCT p.menu_id FROM permissions p "
        "JOIN role_permissions rp ON rp.permission_id = p.id "
        "JOIN user_roles ur ON ur.role_id = rp.role_id "
        "WHERE ur.user_id = ? AND p.menu_id > 0",
        userId);
    std::unordered_set<int64_t> allow;
    for (const auto& row : r) allow.insert(row["menu_id"].as<int64_t>());

    // 2. 全部菜单
    auto all = co_await listMenus();
    std::unordered_map<int64_t, const dto::MenuDto*> byId;
    for (const auto& m : all) byId[m.id] = &m;

    // 3. 祖先链展开：确保父目录也在 allow 里
    bool changed = true;
    while (changed) {
        changed = false;
        std::vector<int64_t> toAdd;
        for (auto id : allow) {
            auto it = byId.find(id);
            if (it != byId.end() && it->second->parentId > 0
                && !allow.count(it->second->parentId)) {
                toAdd.push_back(it->second->parentId);
            }
        }
        for (auto id : toAdd) { allow.insert(id); changed = true; }
    }

    // 4. 过滤 + 构造树
    std::vector<dto::MenuDto> filtered;
    filtered.reserve(allow.size());
    for (const auto& m : all) {
        if (m.visible && allow.count(m.id)) filtered.push_back(m);
    }
    co_return buildTree(std::move(filtered));
}

std::vector<dto::MenuDto> RbacRepository::buildTree(std::vector<dto::MenuDto> flat) {
    // 按 sort 排序
    std::sort(flat.begin(), flat.end(),
              [](const dto::MenuDto& a, const dto::MenuDto& b) {
                  if (a.sort != b.sort) return a.sort < b.sort;
                  return a.id < b.id;
              });

    std::unordered_map<int64_t, dto::MenuDto> byId;
    for (auto& m : flat) byId.emplace(m.id, std::move(m));

    std::vector<dto::MenuDto> roots;
    std::unordered_map<int64_t, std::vector<int64_t>> childIds;
    for (const auto& [id, m] : byId) {
        if (m.parentId > 0 && byId.count(m.parentId)) {
            childIds[m.parentId].push_back(id);
        }
    }
    std::function<dto::MenuDto(int64_t)> take = [&](int64_t id) {
        auto node = byId[id];
        auto itc = childIds.find(id);
        if (itc != childIds.end()) {
            auto ids = itc->second;
            std::sort(ids.begin(), ids.end(),
                      [&](int64_t a, int64_t b) {
                          const auto& ma = byId[a];
                          const auto& mb = byId[b];
                          if (ma.sort != mb.sort) return ma.sort < mb.sort;
                          return ma.id < mb.id;
                      });
            for (auto cid : ids) node.children.push_back(take(cid));
        }
        return node;
    };

    std::vector<int64_t> rootIds;
    for (const auto& [id, m] : byId) {
        if (m.parentId == 0 || !byId.count(m.parentId)) rootIds.push_back(id);
    }
    std::sort(rootIds.begin(), rootIds.end(),
              [&](int64_t a, int64_t b) {
                  const auto& ma = byId[a];
                  const auto& mb = byId[b];
                  if (ma.sort != mb.sort) return ma.sort < mb.sort;
                  return ma.id < mb.id;
              });
    for (auto id : rootIds) roots.push_back(take(id));
    return roots;
}

} // namespace modules::rbac
