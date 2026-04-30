#include "UserController.h"
#include "core/Result.h"
#include "core/Logger.h"
#include "dto/CreateUserReq.h"
#include "dto/LoginReq.h"
#include "dto/UpdateUserReq.h"
#include "common/TimeUtil.h"
#include "modules/rbac/RbacService.h"
#include "modules/rbac/dto/RbacReq.h"
#include "filters/PermissionRegistry.h"

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
        auto u = co_await svc_.create(*reqDto);
        APP_LOG_INFO << "user created id=" << u.id;
        cb(core::Result::ok(u.toJson(), "created"));
    } catch (const std::exception& e) {
        std::string msg = e.what();
        if (isUniqueViolation(msg)) {
            cb(core::Result::fail(4090, "email already registered",
                                  drogon::k409Conflict));
        } else {
            cb(core::Result::fail(5002, "db error: " + msg,
                                  drogon::k500InternalServerError));
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
        APP_LOG_INFO << "admin create user id=" << u.id << " email=" << u.email;
        cb(core::Result::ok(u.toJson(), "created"));
    } catch (const std::exception& e) {
        std::string msg = e.what();
        if (isUniqueViolation(msg)) {
            cb(core::Result::fail(4090, "email already registered",
                                  drogon::k409Conflict));
        } else {
            cb(core::Result::fail(5002, "db error: " + msg,
                                  drogon::k500InternalServerError));
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
    try {
        auto u = co_await svc_.update(id, *reqDto);
        if (!u) {
            cb(core::Result::notFound("user not found"));
            co_return;
        }
        APP_LOG_INFO << "user updated id=" << u->id << " email=" << u->email;
        cb(core::Result::ok(u->toJson(), "updated"));
    } catch (const std::exception& e) {
        std::string msg = e.what();
        if (isUniqueViolation(msg)) {
            cb(core::Result::fail(4090, "email already used",
                                  drogon::k409Conflict));
        } else {
            cb(core::Result::fail(5002, "db error: " + msg,
                                  drogon::k500InternalServerError));
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
    } catch (const LoginInvalidError& e) {
        cb(core::Result::fail(4011, e.what(), drogon::k401Unauthorized));
    } catch (const std::exception& e) {
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
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
        cb(core::Result::fail(5001, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
    }
}

drogon::AsyncTask UserController::list(
    drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr&)> cb) {
    int page     = 1;
    int pageSize = 10;
    std::string keyword;
    try {
        auto& params = req->getParameters();
        if (auto it = params.find("page");      it != params.end()) page = std::stoi(it->second);
        if (auto it = params.find("page_size"); it != params.end()) pageSize = std::stoi(it->second);
        if (auto it = params.find("keyword");   it != params.end()) keyword = it->second;
    } catch (...) { /* 保持默认值 */ }

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
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
    }
}

drogon::AsyncTask UserController::getById(
    drogon::HttpRequestPtr /*req*/,
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
        cb(core::Result::fail(5001, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
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
        cb(core::Result::ok(Json::nullValue, "deleted"));
    } catch (const std::exception& e) {
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
    }
}

drogon::AsyncTask UserController::getRoles(
    drogon::HttpRequestPtr /*req*/,
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
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
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
        cb(core::Result::ok(Json::nullValue, "ok"));
    } catch (const std::exception& e) {
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
    }
}

void UserController::health(const drogon::HttpRequestPtr& /*req*/,
                            std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    Json::Value data;
    data["status"] = "ok";
    data["time"]   = common::TimeUtil::formatLocal(common::TimeUtil::nowSec());
    cb(core::Result::ok(std::move(data)));
}

} // namespace modules::user
