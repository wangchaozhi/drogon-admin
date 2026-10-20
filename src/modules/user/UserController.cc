#include "UserController.h"
#include "core/DomainError.h"
#include "core/ErrorHandler.h"
#include "core/Result.h"
#include "core/Logger.h"
#include "dto/ChangePasswordReq.h"
#include "dto/CreateUserReq.h"
#include "dto/LoginReq.h"
#include "dto/UpdateUserReq.h"
#include "common/TimeUtil.h"
#include "modules/audit/AuditService.h"
#include "modules/rbac/RbacService.h"
#include "modules/rbac/dto/RbacReq.h"
#include "filters/PermissionRegistry.h"
#include <stdexcept>

namespace modules::user {

namespace {

Json::Value toJsonArr(const std::vector<std::string>& v) {
    Json::Value arr(Json::arrayValue);
    for (const auto& s : v) arr.append(s);
    return arr;
}

Json::Value menusToJson(const std::vector<rbac::dto::MenuDto>& tree) {
    Json::Value arr(Json::arrayValue);
    for (const auto& m : tree) arr.append(m.toJson());
    return arr;
}

bool isUniqueViolation(const std::string& msg) {
    return msg.find("UNIQUE") != std::string::npos
        || msg.find("unique") != std::string::npos;
}

} // namespace

UserController::UserController() {
    auto& reg = filters::PermissionRegistry::instance();
    reg.bind("GET",    "/api/users",                       "user:view");
    reg.bind("POST",   "/api/users",                       "user:create");
    reg.bind("GET",    "/api/users/(\\d+)",                "user:view");
    reg.bind("PUT",    "/api/users/(\\d+)",                "user:update");
    reg.bind("DELETE", "/api/users/(\\d+)",                "user:delete");
    reg.bind("GET",    "/api/users/(\\d+)/roles",          "user:view");
    reg.bind("PUT",    "/api/users/(\\d+)/roles",          "user:assign-role");
}

drogon::AsyncTask UserController::registerUser(
    drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr&)> cb) {
    auto json = req->getJsonObject();
    if (!json) {
        cb(core::Result::fail(4001, "invalid json body"));
        co_return;
    }
    std::string err;
    auto reqDto = dto::CreateUserReq::parse(*json, err);
    if (!reqDto) {
        cb(core::Result::fail(4002, err));
        co_return;
    }
    try {
        auto u = co_await svc_.registerPublic(*reqDto);
        Json::Value details;
        details["name"] = u.name;
        details["email"] = u.email;
        details["source"] = "public_registration";
        co_await audit::AuditService::instance().record(
            req,
            "user.create",
            "user",
            std::to_string(u.id),
            std::move(details),
            u.id);
        APP_LOG_INFO << "user created id=" << u.id;
        cb(core::Result::ok(u.toJson(), "created"));
    } catch (const RegistrationClosedError& e) {
        cb(core::Result::fail(4031, e.what(), drogon::k403Forbidden));
    } catch (const std::exception& e) {
        std::string msg = e.what();
        if (isUniqueViolation(msg)) {
            cb(core::Result::fail(4090, "email already registered",
                                  drogon::k409Conflict));
        } else {
            cb(core::internalError(req, "register user", e, 5002));
        }
    }
}

drogon::AsyncTask UserController::adminCreate(
    drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr&)> cb) {
    auto json = req->getJsonObject();
    if (!json) {
        cb(core::Result::fail(4001, "invalid json body"));
        co_return;
    }
    std::string err;
    auto reqDto = dto::CreateUserReq::parse(*json, err);
    if (!reqDto) {
        cb(core::Result::fail(4002, err));
        co_return;
    }
    try {
        auto u = co_await svc_.create(*reqDto);
        Json::Value details;
        details["name"] = u.name;
        details["email"] = u.email;
        details["source"] = "admin";
        co_await audit::AuditService::instance().record(
            req,
            "user.create",
            "user",
            std::to_string(u.id),
            std::move(details));
        APP_LOG_INFO << "admin create user id=" << u.id << " email=" << u.email;
        cb(core::Result::ok(u.toJson(), "created"));
    } catch (const std::exception& e) {
        std::string msg = e.what();
        if (isUniqueViolation(msg)) {
            cb(core::Result::fail(4090, "email already registered",
                                  drogon::k409Conflict));
        } else {
            cb(core::internalError(req, "create user", e, 5002));
        }
    }
}

drogon::AsyncTask UserController::update(
    drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr&)> cb,
    int64_t id) {
    auto json = req->getJsonObject();
    if (!json) {
        cb(core::Result::fail(4001, "invalid json body"));
        co_return;
    }
    std::string err;
    auto reqDto = dto::UpdateUserReq::parse(*json, err);
    if (!reqDto) {
        cb(core::Result::fail(4002, err));
        co_return;
    }
    const auto attrs = req->attributes();
    const int64_t currentUid = attrs && attrs->find("userId")
        ? attrs->get<int64_t>("userId") : 0;
    if (currentUid == id && reqDto->disabled.value_or(false)) {
        cb(core::Result::fail(
            4003, "cannot disable yourself", drogon::k400BadRequest));
        co_return;
    }
    try {
        auto u = co_await svc_.update(id, *reqDto);
        if (!u) {
            cb(core::Result::notFound("user not found"));
            co_return;
        }
        Json::Value details;
        details["name"] = u->name;
        details["email"] = u->email;
        details["disabled"] = u->disabled;
        details["password_changed"] = reqDto->password.has_value();
        co_await audit::AuditService::instance().record(
            req,
            "user.update",
            "user",
            std::to_string(id),
            std::move(details));
        APP_LOG_INFO << "user updated id=" << u->id << " email=" << u->email;
        cb(core::Result::ok(u->toJson(), "updated"));
    } catch (const core::ConflictError& e) {
        cb(core::Result::fail(4091, e.what(), drogon::k409Conflict));
    } catch (const std::exception& e) {
        std::string msg = e.what();
        if (isUniqueViolation(msg)) {
            cb(core::Result::fail(4090, "email already used",
                                  drogon::k409Conflict));
        } else {
            cb(core::internalError(req, "update user", e, 5002));
        }
    }
}

drogon::AsyncTask UserController::login(
    drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr&)> cb) {
    auto json = req->getJsonObject();
    if (!json) {
        cb(core::Result::fail(4001, "invalid json body"));
        co_return;
    }
    std::string err;
    auto reqDto = dto::LoginReq::parse(*json, err);
    if (!reqDto) {
        cb(core::Result::fail(4002, err));
        co_return;
    }
    try {
        auto r = co_await svc_.login(*reqDto);
        Json::Value data;
        data["token"]       = r.token;
        data["token_type"]  = "Bearer";
        data["expires_at"]  = static_cast<Json::Int64>(r.expiresAt);
        data["user"]        = r.user.toJson();
        data["roles"]       = toJsonArr(r.roles);
        data["permissions"] = toJsonArr(r.permissions);
        data["menus"]       = menusToJson(r.menus);
        APP_LOG_INFO << "user login id=" << r.user.id
                     << " roles=" << r.roles.size()
                     << " perms=" << r.permissions.size();
        cb(core::Result::ok(std::move(data), "login ok"));
    } catch (const LoginThrottledError& e) {
        auto resp = core::Result::fail(
            4291, e.what(), drogon::k429TooManyRequests);
        resp->addHeader("Retry-After", std::to_string(e.retryAfter()));
        cb(resp);
    } catch (const LoginInvalidError& e) {
        cb(core::Result::fail(4011, e.what(), drogon::k401Unauthorized));
    } catch (const std::exception& e) {
        cb(core::internalError(req, "login", e, 5003));
    }
}

drogon::AsyncTask UserController::logout(
    drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr&)> cb) {
    const auto attrs = req->attributes();
    if (!attrs || !attrs->find("userId")) {
        cb(core::Result::unauthorized("missing auth context"));
        co_return;
    }
    const auto userId = attrs->get<int64_t>("userId");
    try {
        co_await svc_.revokeSessions(userId);
        Json::Value details;
        details["all_sessions_revoked"] = true;
        co_await audit::AuditService::instance().record(
            req,
            "auth.logout",
            "user",
            std::to_string(userId),
            std::move(details));
        cb(core::Result::ok(Json::nullValue, "logged out"));
    } catch (const std::exception& error) {
        cb(core::internalError(req, "logout", error, 5003));
    }
}

drogon::AsyncTask UserController::me(
    drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr&)> cb) {
    auto attrs = req->attributes();
    if (!attrs || !attrs->find("userId")) {
        cb(core::Result::unauthorized("missing auth context"));
        co_return;
    }
    int64_t uid = attrs->get<int64_t>("userId");
    try {
        auto u = co_await svc_.getById(uid);
        if (!u) {
            cb(core::Result::notFound("user not found"));
            co_return;
        }
        Json::Value data;
        data["user"] = u->toJson();
        try {
            auto view  = co_await rbac::RbacService::instance().getUserView(uid);
            auto menus = co_await rbac::RbacService::instance().getUserMenus(uid);
            data["roles"]       = toJsonArr(view.roles);
            data["permissions"] = toJsonArr(view.perms);
            data["menus"]       = menusToJson(menus);
        } catch (const std::exception& e) {
            APP_LOG_ERROR << "me fetch rbac failed: " << e.what();
            data["roles"]       = Json::Value(Json::arrayValue);
            data["permissions"] = Json::Value(Json::arrayValue);
            data["menus"]       = Json::Value(Json::arrayValue);
        }
        cb(core::Result::ok(std::move(data)));
    } catch (const std::exception& e) {
        cb(core::internalError(req, "load current user", e, 5001));
    }
}

drogon::AsyncTask UserController::changePassword(
    drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr&)> cb) {
    const auto attrs = req->attributes();
    if (!attrs || !attrs->find("userId")) {
        cb(core::Result::unauthorized("missing auth context"));
        co_return;
    }

    const auto json = req->getJsonObject();
    if (!json) {
        cb(core::Result::fail(4001, "invalid json body"));
        co_return;
    }
    std::string error;
    auto request = dto::ChangePasswordReq::parse(*json, error);
    if (!request) {
        cb(core::Result::fail(4002, error));
        co_return;
    }

    try {
        co_await svc_.changePassword(
            attrs->get<int64_t>("userId"), std::move(*request));
        Json::Value details;
        details["password_changed"] = true;
        co_await audit::AuditService::instance().record(
            req,
            "user.change_password",
            "user",
            std::to_string(attrs->get<int64_t>("userId")),
            std::move(details));
        cb(core::Result::ok(
            Json::nullValue, "password changed; please sign in again"));
    } catch (const CurrentPasswordInvalidError& exception) {
        cb(core::Result::fail(
            4003, exception.what(), drogon::k400BadRequest));
    } catch (const std::exception& exception) {
        cb(core::internalError(
            req, "change current user password", exception, 5003));
    }
}

drogon::AsyncTask UserController::list(
    drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr&)> cb) {
    int page     = 1;
    int pageSize = 10;
    std::string keyword;
    try {
        const auto parseInteger = [](const std::string& value) {
            size_t parsed = 0;
            const int result = std::stoi(value, &parsed);
            if (parsed != value.size())
                throw std::invalid_argument("trailing characters");
            return result;
        };
        auto& params = req->getParameters();
        if (auto it = params.find("page"); it != params.end())
            page = parseInteger(it->second);
        if (auto it = params.find("page_size"); it != params.end())
            pageSize = parseInteger(it->second);
        if (auto it = params.find("keyword");   it != params.end()) keyword = it->second;
    } catch (const std::exception&) {
        cb(core::Result::fail(
            4002, "page and page_size must be integers",
            drogon::k400BadRequest));
        co_return;
    }
    if (page < 1 || page > 1000000) {
        cb(core::Result::fail(
            4002, "page must be between 1 and 1000000",
            drogon::k400BadRequest));
        co_return;
    }
    if (pageSize < 1 || pageSize > 200) {
        cb(core::Result::fail(
            4002, "page_size must be between 1 and 200",
            drogon::k400BadRequest));
        co_return;
    }
    if (keyword.size() > 100) {
        cb(core::Result::fail(
            4002, "keyword must not exceed 100 characters",
            drogon::k400BadRequest));
        co_return;
    }

    try {
        auto p = co_await svc_.repo().listPaged(page, pageSize, keyword);
        Json::Value items(Json::arrayValue);
        for (const auto& u : p.items) items.append(u.toJson());
        Json::Value data;
        data["items"]     = items;
        data["total"]     = static_cast<Json::Int64>(p.total);
        data["page"]      = page;
        data["page_size"] = pageSize;
        cb(core::Result::ok(std::move(data)));
    } catch (const std::exception& e) {
        cb(core::internalError(req, "list users", e, 5003));
    }
}

drogon::AsyncTask UserController::getById(
    drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr&)> cb,
    int64_t id) {
    APP_LOG_DEBUG << "getById id=" << id;
    try {
        auto u = co_await svc_.getById(id);
        if (!u) {
            cb(core::Result::notFound("user not found"));
            co_return;
        }
        cb(core::Result::ok(u->toJson()));
    } catch (const std::exception& e) {
        cb(core::internalError(req, "get user", e, 5001));
    }
}

drogon::AsyncTask UserController::remove(
    drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr&)> cb,
    int64_t id) {
    auto attrs = req->attributes();
    int64_t currentUid = attrs && attrs->find("userId")
                         ? attrs->get<int64_t>("userId") : 0;
    if (currentUid == id) {
        cb(core::Result::fail(4003, "cannot delete yourself"));
        co_return;
    }
    try {
        bool ok = co_await svc_.repo().deleteById(id);
        if (!ok) {
            cb(core::Result::notFound("user not found"));
            co_return;
        }
        rbac::RbacService::instance().invalidateUser(id);
        co_await audit::AuditService::instance().record(
            req,
            "user.delete",
            "user",
            std::to_string(id));
        cb(core::Result::ok(Json::nullValue, "deleted"));
    } catch (const core::ConflictError& e) {
        cb(core::Result::fail(4091, e.what(), drogon::k409Conflict));
    } catch (const std::exception& e) {
        cb(core::internalError(req, "delete user", e, 5003));
    }
}

drogon::AsyncTask UserController::getRoles(
    drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr&)> cb,
    int64_t id) {
    try {
        auto ids = co_await rbac::RbacService::instance().repo().getUserRoleIds(id);
        Json::Value arr(Json::arrayValue);
        for (auto v : ids) arr.append(static_cast<Json::Int64>(v));
        Json::Value data;
        data["role_ids"] = arr;
        cb(core::Result::ok(data));
    } catch (const std::exception& e) {
        cb(core::internalError(req, "get user roles", e, 5003));
    }
}

drogon::AsyncTask UserController::setRoles(
    drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr&)> cb,
    int64_t id) {
    auto json = req->getJsonObject();
    if (!json) { cb(core::Result::fail(4001, "invalid json body")); co_return; }
    std::string err;
    auto r = rbac::dto::IdListReq::parse(*json, "role_ids", err);
    if (!r) { cb(core::Result::fail(4002, err)); co_return; }
    try {
        co_await rbac::RbacService::instance().repo().setUserRoles(id, r->ids);
        rbac::RbacService::instance().invalidateUser(id);
        Json::Value details;
        Json::Value roleIds(Json::arrayValue);
        for (const auto roleId : r->ids)
            roleIds.append(static_cast<Json::Int64>(roleId));
        details["role_ids"] = std::move(roleIds);
        co_await audit::AuditService::instance().record(
            req,
            "user.assign_roles",
            "user",
            std::to_string(id),
            std::move(details));
        cb(core::Result::ok(Json::nullValue, "ok"));
    } catch (const core::ConflictError& e) {
        cb(core::Result::fail(4091, e.what(), drogon::k409Conflict));
    } catch (const core::NotFoundError& e) {
        cb(core::Result::notFound(e.what()));
    } catch (const core::ValidationError& e) {
        cb(core::Result::fail(4002, e.what(), drogon::k400BadRequest));
    } catch (const std::exception& e) {
        cb(core::internalError(req, "set user roles", e, 5003));
    }
}

void UserController::health(const drogon::HttpRequestPtr& /*req*/,
                            std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    Json::Value data;
    data["status"] = "ok";
    data["time"]   = common::TimeUtil::formatLocal(common::TimeUtil::nowSec());
    cb(core::Result::ok(std::move(data)));
}

drogon::AsyncTask UserController::ready(
    drogon::HttpRequestPtr /*req*/,
    std::function<void(const drogon::HttpResponsePtr&)> cb) {
    try {
        auto db = drogon::app().getDbClient();
        if (!db) throw std::runtime_error("database client unavailable");
        auto rows = co_await db->execSqlCoro(
            "SELECT COUNT(*) AS migration_count FROM schema_migrations");
        if (rows.empty())
            throw std::runtime_error("migration state unavailable");
        auto journalMode =
            co_await db->execSqlCoro("PRAGMA journal_mode");
        auto foreignKeys =
            co_await db->execSqlCoro("PRAGMA foreign_keys");
        auto busyTimeout =
            co_await db->execSqlCoro("PRAGMA busy_timeout");
        if (journalMode.empty()
            || journalMode[0][0].as<std::string>() != "wal"
            || foreignKeys.empty()
            || foreignKeys[0][0].as<int>() != 1
            || busyTimeout.empty()
            || busyTimeout[0][0].as<int>() < 5000) {
            throw std::runtime_error("database safety pragmas unavailable");
        }

        Json::Value data;
        data["status"] = "ready";
        data["database"] = "ok";
        data["journal_mode"] = "wal";
        data["foreign_keys"] = true;
        data["busy_timeout_ms"] = busyTimeout[0][0].as<int>();
        data["migration_count"] =
            static_cast<Json::Int64>(
                rows[0]["migration_count"].as<int64_t>());
        cb(core::Result::ok(std::move(data)));
    } catch (const std::exception& error) {
        APP_LOG_ERROR << "readiness check failed: " << error.what();
        cb(core::Result::fail(
            5030, "service is not ready",
            drogon::k503ServiceUnavailable));
    }
}

} // namespace modules::user
