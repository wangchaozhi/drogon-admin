#include "UserRepository.h"
#include "common/TimeUtil.h"
#include "core/Logger.h"
#include <drogon/drogon.h>
#include <drogon/orm/Exception.h>
#include <drogon/orm/Result.h>
#include <drogon/orm/Row.h>

namespace modules::user {

drogon::orm::DbClientPtr UserRepository::db() const {
    return drogon::app().getDbClient();
}

drogon::Task<std::optional<dto::UserDto>>
UserRepository::findById(int64_t id) {
    auto client = db();
    if (!client) co_return std::nullopt;

    auto r = co_await client->execSqlCoro(
        "SELECT id,name,email,created_at FROM users WHERE id=?", id);
    if (r.empty()) co_return std::nullopt;
    const auto& row = r[0];
    dto::UserDto u;
    u.id        = row["id"].as<int64_t>();
    u.name      = row["name"].as<std::string>();
    u.email     = row["email"].as<std::string>();
    u.createdAt = row["created_at"].as<int64_t>();
    co_return u;
}

drogon::Task<std::optional<UserRecord>>
UserRepository::findByEmail(std::string email) {
    auto client = db();
    if (!client) co_return std::nullopt;

    auto r = co_await client->execSqlCoro(
        "SELECT id,name,email,password_hash,created_at FROM users WHERE email=?",
        email);
    if (r.empty()) co_return std::nullopt;
    const auto& row = r[0];
    UserRecord rec;
    rec.user.id        = row["id"].as<int64_t>();
    rec.user.name      = row["name"].as<std::string>();
    rec.user.email     = row["email"].as<std::string>();
    rec.user.createdAt = row["created_at"].as<int64_t>();
    rec.passwordHash   = row["password_hash"].as<std::string>();
    co_return rec;
}

drogon::Task<dto::UserDto>
UserRepository::insert(std::string name,
                       std::string email,
                       std::string passwordHash) {
    auto client = db();
    if (!client) throw std::runtime_error("db client unavailable");

    int64_t now = common::TimeUtil::nowSec();
    auto r = co_await client->execSqlCoro(
        "INSERT INTO users(name,email,password_hash,created_at) VALUES(?,?,?,?)",
        name, email, passwordHash, now);

    dto::UserDto u;
    u.id        = r.insertId();
    u.name      = std::move(name);
    u.email     = std::move(email);
    u.createdAt = now;
    co_return u;
}

drogon::Task<UserRepository::PageResult>
UserRepository::listPaged(int page, int pageSize, std::string keyword) {
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 10;
    if (pageSize > 200) pageSize = 200;
    int offset = (page - 1) * pageSize;

    PageResult out;
    auto client = db();
    if (!client) co_return out;

    if (keyword.empty()) {
        auto r = co_await client->execSqlCoro(
            "SELECT id,name,email,created_at FROM users "
            "ORDER BY id DESC LIMIT ? OFFSET ?",
            pageSize, offset);
        for (const auto& row : r) {
            dto::UserDto u;
            u.id        = row["id"].as<int64_t>();
            u.name      = row["name"].as<std::string>();
            u.email     = row["email"].as<std::string>();
            u.createdAt = row["created_at"].as<int64_t>();
            out.items.push_back(std::move(u));
        }
        auto rc = co_await client->execSqlCoro(
            "SELECT COUNT(*) AS c FROM users");
        if (!rc.empty()) out.total = rc[0]["c"].as<int64_t>();
    } else {
        std::string like = "%" + keyword + "%";
        auto r = co_await client->execSqlCoro(
            "SELECT id,name,email,created_at FROM users "
            "WHERE name LIKE ? OR email LIKE ? "
            "ORDER BY id DESC LIMIT ? OFFSET ?",
            like, like, pageSize, offset);
        for (const auto& row : r) {
            dto::UserDto u;
            u.id        = row["id"].as<int64_t>();
            u.name      = row["name"].as<std::string>();
            u.email     = row["email"].as<std::string>();
            u.createdAt = row["created_at"].as<int64_t>();
            out.items.push_back(std::move(u));
        }
        auto rc = co_await client->execSqlCoro(
            "SELECT COUNT(*) AS c FROM users WHERE name LIKE ? OR email LIKE ?",
            like, like);
        if (!rc.empty()) out.total = rc[0]["c"].as<int64_t>();
    }
    co_return out;
}

drogon::Task<bool> UserRepository::deleteById(int64_t id) {
    auto client = db();
    if (!client) co_return false;
    // 一并清理用户-角色关联
    co_await client->execSqlCoro("DELETE FROM user_roles WHERE user_id=?", id);
    auto r = co_await client->execSqlCoro("DELETE FROM users WHERE id=?", id);
    co_return r.affectedRows() > 0;
}

drogon::Task<bool> UserRepository::hasAnyAdmin() {
    auto client = db();
    if (!client) co_return true;   // 拿不到 DbClient 时保守：不提升权限
    auto r = co_await client->execSqlCoro(
        "SELECT COUNT(*) AS c FROM user_roles WHERE role_id=1");
    if (r.empty()) co_return false;
    co_return r[0]["c"].as<int64_t>() > 0;
}

} // namespace modules::user
