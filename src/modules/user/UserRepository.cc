#include "UserRepository.h"
#include "common/TimeUtil.h"
#include "core/Logger.h"
#include <drogon/drogon.h>
#include <drogon/orm/Exception.h>
#include <drogon/orm/Result.h>
#include <drogon/orm/Row.h>

namespace modules::user {

UserRepository::UserRepository() = default;

drogon::orm::DbClientPtr UserRepository::db() const {
    return drogon::app().getDbClient();
}

void UserRepository::findById(int64_t id,
                              std::function<void(std::optional<dto::UserDto>)> onOk,
                              DbErrCb onErr) {
    auto client = db();
    if (!client) { onErr("db client unavailable"); return; }

    client->execSqlAsync(
        "SELECT id,name,email,created_at FROM users WHERE id=?",
        [onOk = std::move(onOk)](const drogon::orm::Result& r) {
            if (r.empty()) { onOk(std::nullopt); return; }
            const auto& row = r[0];
            dto::UserDto u;
            u.id        = row["id"].as<int64_t>();
            u.name      = row["name"].as<std::string>();
            u.email     = row["email"].as<std::string>();
            u.createdAt = row["created_at"].as<int64_t>();
            onOk(std::move(u));
        },
        [onErr = std::move(onErr)](const drogon::orm::DrogonDbException& e) {
            APP_LOG_ERROR << "findById failed: " << e.base().what();
            onErr(e.base().what());
        },
        id);
}

void UserRepository::findByEmail(const std::string& email,
                                 std::function<void(std::optional<UserRecord>)> onOk,
                                 DbErrCb onErr) {
    auto client = db();
    if (!client) { onErr("db client unavailable"); return; }

    client->execSqlAsync(
        "SELECT id,name,email,password_hash,created_at FROM users WHERE email=?",
        [onOk = std::move(onOk)](const drogon::orm::Result& r) {
            if (r.empty()) { onOk(std::nullopt); return; }
            const auto& row = r[0];
            UserRecord rec;
            rec.user.id        = row["id"].as<int64_t>();
            rec.user.name      = row["name"].as<std::string>();
            rec.user.email     = row["email"].as<std::string>();
            rec.user.createdAt = row["created_at"].as<int64_t>();
            rec.passwordHash   = row["password_hash"].as<std::string>();
            onOk(std::move(rec));
        },
        [onErr = std::move(onErr)](const drogon::orm::DrogonDbException& e) {
            APP_LOG_ERROR << "findByEmail failed: " << e.base().what();
            onErr(e.base().what());
        },
        email);
}

void UserRepository::insert(const std::string& name,
                            const std::string& email,
                            const std::string& passwordHash,
                            std::function<void(dto::UserDto)> onOk,
                            DbErrCb onErr) {
    auto client = db();
    if (!client) { onErr("db client unavailable"); return; }

    int64_t now = common::TimeUtil::nowSec();

    client->execSqlAsync(
        "INSERT INTO users(name,email,password_hash,created_at) VALUES(?,?,?,?)",
        [onOk = std::move(onOk), name, email, now](const drogon::orm::Result& r) {
            dto::UserDto u;
            u.id        = r.insertId();
            u.name      = name;
            u.email     = email;
            u.createdAt = now;
            onOk(std::move(u));
        },
        [onErr = std::move(onErr)](const drogon::orm::DrogonDbException& e) {
            APP_LOG_ERROR << "insert user failed: " << e.base().what();
            onErr(e.base().what());
        },
        name, email, passwordHash, now);
}

UserRepository::PageResult
UserRepository::listPagedSync(int page, int pageSize, const std::string& keyword) {
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 10;
    if (pageSize > 200) pageSize = 200;
    int offset = (page - 1) * pageSize;

    auto client = db();
    PageResult out;
    if (!client) return out;

    std::string like = "%" + keyword + "%";
    if (keyword.empty()) {
        auto r = client->execSqlSync(
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
        auto rc = client->execSqlSync("SELECT COUNT(*) AS c FROM users");
        if (!rc.empty()) out.total = rc[0]["c"].as<int64_t>();
    } else {
        auto r = client->execSqlSync(
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
        auto rc = client->execSqlSync(
            "SELECT COUNT(*) AS c FROM users WHERE name LIKE ? OR email LIKE ?",
            like, like);
        if (!rc.empty()) out.total = rc[0]["c"].as<int64_t>();
    }
    return out;
}

bool UserRepository::deleteByIdSync(int64_t id) {
    auto client = db();
    if (!client) return false;
    // 一并清理用户-角色关联
    client->execSqlSync("DELETE FROM user_roles WHERE user_id=?", id);
    auto r = client->execSqlSync("DELETE FROM users WHERE id=?", id);
    return r.affectedRows() > 0;
}

} // namespace modules::user
