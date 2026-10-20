#include "UserRepository.h"
#include "common/TimeUtil.h"
#include "core/Logger.h"
#include <drogon/drogon.h>
#include <drogon/orm/Exception.h>
#include <drogon/orm/Result.h>
#include <drogon/orm/Row.h>

namespace modules::user {

namespace {

dto::UserDto userFromRow(const drogon::orm::Row& row) {
    dto::UserDto user;
    user.id        = row["id"].as<int64_t>();
    user.name      = row["name"].as<std::string>();
    user.email     = row["email"].as<std::string>();
    user.disabled  = row["disabled"].as<int>() != 0;
    user.createdAt = row["created_at"].as<int64_t>();
    return user;
}

} // namespace

drogon::orm::DbClientPtr UserRepository::db() const {
    return drogon::app().getDbClient();
}

drogon::Task<std::optional<dto::UserDto>>
UserRepository::findById(int64_t id) {
    auto client = db();
    if (!client) co_return std::nullopt;

    auto r = co_await client->execSqlCoro(
        "SELECT id,name,email,disabled,created_at FROM users WHERE id=?", id);
    if (r.empty()) co_return std::nullopt;
    co_return userFromRow(r[0]);
}

drogon::Task<std::optional<UserRecord>>
UserRepository::findByEmail(std::string email) {
    auto client = db();
    if (!client) co_return std::nullopt;

    auto r = co_await client->execSqlCoro(
        "SELECT id,name,email,password_hash,disabled,token_version,"
        "failed_login_count,locked_until,created_at "
        "FROM users WHERE email=?",
        email);
    if (r.empty()) co_return std::nullopt;
    const auto& row = r[0];
    UserRecord rec;
    rec.user             = userFromRow(row);
    rec.passwordHash     = row["password_hash"].as<std::string>();
    rec.tokenVersion     = row["token_version"].as<int64_t>();
    rec.failedLoginCount = row["failed_login_count"].as<int>();
    rec.lockedUntil      = row["locked_until"].as<int64_t>();
    co_return rec;
}

drogon::Task<std::optional<AuthState>>
UserRepository::findAuthState(int64_t id) {
    auto client = db();
    if (!client) co_return std::nullopt;
    auto r = co_await client->execSqlCoro(
        "SELECT disabled,token_version FROM users WHERE id=?", id);
    if (r.empty()) co_return std::nullopt;
    AuthState state;
    state.disabled = r[0]["disabled"].as<int>() != 0;
    state.tokenVersion = r[0]["token_version"].as<int64_t>();
    co_return state;
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

drogon::Task<dto::UserDto>
UserRepository::registerWithDefaultRole(std::string name,
                                        std::string email,
                                        std::string passwordHash,
                                        bool firstUserOnly) {
    auto client = db();
    if (!client) throw std::runtime_error("db client unavailable");

    auto tx = co_await client->newTransactionCoro(
        drogon::orm::TransactionType::Immediate);
    auto admins = co_await tx->execSqlCoro(
        "SELECT COUNT(*) AS c FROM user_roles WHERE role_id=1");
    const bool hasAdmin =
        !admins.empty() && admins[0]["c"].as<int64_t>() > 0;
    if (firstUserOnly && hasAdmin)
        throw std::runtime_error("registration_closed");

    const int64_t now = common::TimeUtil::nowSec();
    auto inserted = co_await tx->execSqlCoro(
        "INSERT INTO users(name,email,password_hash,created_at) VALUES(?,?,?,?)",
        name, email, passwordHash, now);
    const int64_t userId = inserted.insertId();
    const int64_t roleId = hasAdmin ? 2 : 1;
    co_await tx->execSqlCoro(
        "INSERT INTO user_roles(user_id,role_id) VALUES(?,?)",
        userId, roleId);

    dto::UserDto user;
    user.id = userId;
    user.name = std::move(name);
    user.email = std::move(email);
    user.createdAt = now;
    co_return user;
}

drogon::Task<dto::UserDto>
UserRepository::insertWithRole(std::string name,
                               std::string email,
                               std::string passwordHash,
                               int64_t roleId) {
    auto client = db();
    if (!client) throw std::runtime_error("db client unavailable");

    auto tx = co_await client->newTransactionCoro(
        drogon::orm::TransactionType::Immediate);
    const int64_t now = common::TimeUtil::nowSec();
    auto inserted = co_await tx->execSqlCoro(
        "INSERT INTO users(name,email,password_hash,created_at) VALUES(?,?,?,?)",
        name, email, passwordHash, now);
    const int64_t userId = inserted.insertId();
    co_await tx->execSqlCoro(
        "INSERT INTO user_roles(user_id,role_id) VALUES(?,?)",
        userId, roleId);

    dto::UserDto user;
    user.id = userId;
    user.name = std::move(name);
    user.email = std::move(email);
    user.createdAt = now;
    co_return user;
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
            "SELECT id,name,email,disabled,created_at FROM users "
            "ORDER BY id DESC LIMIT ? OFFSET ?",
            pageSize, offset);
        for (const auto& row : r) {
            out.items.push_back(userFromRow(row));
        }
        auto rc = co_await client->execSqlCoro(
            "SELECT COUNT(*) AS c FROM users");
        if (!rc.empty()) out.total = rc[0]["c"].as<int64_t>();
    } else {
        std::string like = "%" + keyword + "%";
        auto r = co_await client->execSqlCoro(
            "SELECT id,name,email,disabled,created_at FROM users "
            "WHERE name LIKE ? OR email LIKE ? "
            "ORDER BY id DESC LIMIT ? OFFSET ?",
            like, like, pageSize, offset);
        for (const auto& row : r) {
            out.items.push_back(userFromRow(row));
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

drogon::Task<> UserRepository::updatePasswordHash(int64_t id,
                                                  std::string passwordHash,
                                                  bool revokeTokens) {
    auto client = db();
    if (!client) throw std::runtime_error("db client unavailable");
    auto r = revokeTokens
        ? co_await client->execSqlCoro(
              "UPDATE users SET password_hash=?,token_version=token_version+1 "
              "WHERE id=?",
              std::move(passwordHash), id)
        : co_await client->execSqlCoro(
              "UPDATE users SET password_hash=? WHERE id=?",
              std::move(passwordHash), id);
    if (r.affectedRows() != 1)
        throw std::runtime_error("password hash update affected unexpected rows");
    co_return;
}

drogon::Task<LoginAttemptState>
UserRepository::recordLoginFailure(int64_t id,
                                   int maxFailures,
                                   int lockSeconds) {
    auto client = db();
    if (!client) throw std::runtime_error("db client unavailable");

    auto tx = co_await client->newTransactionCoro(
        drogon::orm::TransactionType::Immediate);
    auto rows = co_await tx->execSqlCoro(
        "SELECT failed_login_count,locked_until FROM users WHERE id=?", id);
    if (rows.empty()) co_return LoginAttemptState{};

    const int64_t now = common::TimeUtil::nowSec();
    const int64_t previousLock = rows[0]["locked_until"].as<int64_t>();
    const bool lockExpired = previousLock > 0 && previousLock <= now;
    const int failures = lockExpired
        ? 1
        : rows[0]["failed_login_count"].as<int>() + 1;
    const int64_t lockedUntil =
        failures >= maxFailures ? now + lockSeconds : 0;
    co_await tx->execSqlCoro(
        "UPDATE users SET failed_login_count=?,locked_until=? WHERE id=?",
        failures, lockedUntil, id);
    co_return LoginAttemptState{failures, lockedUntil};
}

drogon::Task<> UserRepository::resetLoginFailures(int64_t id) {
    auto client = db();
    if (!client) throw std::runtime_error("db client unavailable");
    co_await client->execSqlCoro(
        "UPDATE users SET failed_login_count=0,locked_until=0 WHERE id=?", id);
    co_return;
}

drogon::Task<std::optional<dto::UserDto>>
UserRepository::updateById(int64_t id,
                           std::string name,
                           std::string email,
                           std::string passwordHash) {
    auto client = db();
    if (!client) co_return std::nullopt;

    if (passwordHash.empty()) {
        auto r = co_await client->execSqlCoro(
            "UPDATE users SET name=?, email=? WHERE id=?",
            name, email, id);
        if (r.affectedRows() <= 0) co_return std::nullopt;
    } else {
        auto r = co_await client->execSqlCoro(
            "UPDATE users SET name=?, email=?, password_hash=?,"
            "token_version=token_version+1 WHERE id=?",
            name, email, passwordHash, id);
        if (r.affectedRows() <= 0) co_return std::nullopt;
    }
    co_return co_await findById(id);
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
