#include "UserController.h"
#include "core/Result.h"
#include "core/Logger.h"
#include "dto/CreateUserReq.h"
#include "dto/LoginReq.h"
#include "common/TimeUtil.h"
#include "modules/rbac/RbacService.h"
#include "modules/rbac/dto/RbacReq.h"
#include "filters/PermissionRegistry.h"

namespace modules::user {

namespace {
using CbPtr = std::shared_ptr<std::function<void(const drogon::HttpResponsePtr&)>>;

CbPtr shareCb(std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    return std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(cb));
}

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
} // namespace

UserController::UserController() {
    auto& reg = filters::PermissionRegistry::instance();
    reg.bind("GET",    "/api/users",                       "user:view");
    reg.bind("GET",    "/api/users/(\\d+)",                "user:view");
    reg.bind("DELETE", "/api/users/(\\d+)",                "user:delete");
    reg.bind("GET",    "/api/users/(\\d+)/roles",          "user:view");
    reg.bind("PUT",    "/api/users/(\\d+)/roles",          "user:assign-role");
}

void UserController::getById(const drogon::HttpRequestPtr& /*req*/,
                             std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                             int64_t id) {
    APP_LOG_DEBUG << "getById id=" << id;
    auto callback = shareCb(std::move(cb));

    svc_.getById(
        id,
        [callback](std::optional<dto::UserDto> u) {
            if (!u) { (*callback)(core::Result::notFound("user not found")); return; }
            (*callback)(core::Result::ok(u->toJson()));
        },
        [callback](const std::string& err) {
            (*callback)(core::Result::fail(5001, "db error: " + err,
                                           drogon::k500InternalServerError));
        });
}

void UserController::registerUser(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    auto callback = shareCb(std::move(cb));

    auto json = req->getJsonObject();
    if (!json) {
        (*callback)(core::Result::fail(4001, "invalid json body"));
        return;
    }
    std::string err;
    auto reqDto = dto::CreateUserReq::parse(*json, err);
    if (!reqDto) {
        (*callback)(core::Result::fail(4002, err));
        return;
    }

    svc_.create(
        *reqDto,
        [callback](dto::UserDto u) {
            APP_LOG_INFO << "user created id=" << u.id;
            (*callback)(core::Result::ok(u.toJson(), "created"));
        },
        [callback](const std::string& e) {
            int code = (e.find("UNIQUE") != std::string::npos ||
                        e.find("unique") != std::string::npos) ? 4090 : 5002;
            auto http = (code == 4090) ? drogon::k409Conflict
                                       : drogon::k500InternalServerError;
            std::string msg = (code == 4090) ? "email already registered"
                                             : "db error: " + e;
            (*callback)(core::Result::fail(code, msg, http));
        });
}

void UserController::login(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    auto callback = shareCb(std::move(cb));

    auto json = req->getJsonObject();
    if (!json) {
        (*callback)(core::Result::fail(4001, "invalid json body"));
        return;
    }
    std::string err;
    auto reqDto = dto::LoginReq::parse(*json, err);
    if (!reqDto) {
        (*callback)(core::Result::fail(4002, err));
        return;
    }

    svc_.login(
        *reqDto,
        [callback](LoginResult r) {
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
            (*callback)(core::Result::ok(std::move(data), "login ok"));
        },
        [callback](const std::string& msg) {
            (*callback)(core::Result::fail(4011, msg, drogon::k401Unauthorized));
        },
        [callback](const std::string& e) {
            (*callback)(core::Result::fail(5003, "db error: " + e,
                                           drogon::k500InternalServerError));
        });
}

void UserController::me(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    auto callback = shareCb(std::move(cb));

    auto attrs = req->attributes();
    if (!attrs || !attrs->find("userId")) {
        (*callback)(core::Result::unauthorized("missing auth context"));
        return;
    }
    int64_t uid = attrs->get<int64_t>("userId");

    svc_.getById(
        uid,
        [callback, uid](std::optional<dto::UserDto> u) {
            if (!u) { (*callback)(core::Result::notFound("user not found")); return; }
            Json::Value data;
            data["user"] = u->toJson();
            try {
                auto view  = rbac::RbacService::instance().getUserView(uid);
                auto menus = rbac::RbacService::instance().getUserMenus(uid);
                data["roles"]       = toJsonArr(view.roles);
                data["permissions"] = toJsonArr(view.perms);
                data["menus"]       = menusToJson(menus);
            } catch (const std::exception& e) {
                APP_LOG_ERROR << "me fetch rbac failed: " << e.what();
                data["roles"]       = Json::Value(Json::arrayValue);
                data["permissions"] = Json::Value(Json::arrayValue);
                data["menus"]       = Json::Value(Json::arrayValue);
            }
            (*callback)(core::Result::ok(std::move(data)));
        },
        [callback](const std::string& err) {
            (*callback)(core::Result::fail(5001, "db error: " + err,
                                           drogon::k500InternalServerError));
        });
}

void UserController::list(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
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
        auto p = svc_.repo().listPagedSync(page, pageSize, keyword);
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

void UserController::remove(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                            int64_t id) {
    auto attrs = req->attributes();
    int64_t currentUid = attrs && attrs->find("userId")
                         ? attrs->get<int64_t>("userId") : 0;
    if (currentUid == id) {
        cb(core::Result::fail(4003, "cannot delete yourself"));
        return;
    }
    try {
        if (!svc_.repo().deleteByIdSync(id)) {
            cb(core::Result::notFound("user not found"));
            return;
        }
        rbac::RbacService::instance().invalidateUser(id);
        cb(core::Result::ok(Json::nullValue, "deleted"));
    } catch (const std::exception& e) {
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
    }
}

void UserController::getRoles(const drogon::HttpRequestPtr&,
                              std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                              int64_t id) {
    try {
        auto ids = rbac::RbacService::instance().repo().getUserRoleIds(id);
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

void UserController::setRoles(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                              int64_t id) {
    auto json = req->getJsonObject();
    if (!json) { cb(core::Result::fail(4001, "invalid json body")); return; }
    std::string err;
    auto r = rbac::dto::IdListReq::parse(*json, "role_ids", err);
    if (!r) { cb(core::Result::fail(4002, err)); return; }
    try {
        rbac::RbacService::instance().repo().setUserRoles(id, r->ids);
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
