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

void UserRepository::insert(const std::string& name,
                            const std::string& email,
                            std::function<void(dto::UserDto)> onOk,
                            DbErrCb onErr) {
    auto client = db();
    if (!client) { onErr("db client unavailable"); return; }

    int64_t now = common::TimeUtil::nowSec();

    client->execSqlAsync(
        "INSERT INTO users(name,email,created_at) VALUES(?,?,?)",
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
        name, email, now);
}

} // namespace modules::user
