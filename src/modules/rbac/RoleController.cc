#include "RoleController.h"
#include "RbacService.h"
#include "dto/RbacReq.h"
#include "core/Result.h"
#include "core/Logger.h"
#include "filters/PermissionRegistry.h"

namespace modules::rbac {

RoleController::RoleController() {
    auto& reg = filters::PermissionRegistry::instance();
    reg.bind("GET",    "/api/roles",                       "role:view");
    reg.bind("POST",   "/api/roles",                       "role:create");
    reg.bind("PUT",    "/api/roles/(\\d+)",                "role:update");
    reg.bind("DELETE", "/api/roles/(\\d+)",                "role:delete");
    reg.bind("GET",    "/api/roles/(\\d+)/permissions",    "role:view");
    reg.bind("PUT",    "/api/roles/(\\d+)/permissions",    "role:assign");
}

drogon::AsyncTask RoleController::list(
    drogon::HttpRequestPtr /*req*/,
    std::function<void(const drogon::HttpResponsePtr&)> cb) {
    try {
        auto roles = co_await RbacService::instance().repo().listRoles();
        Json::Value arr(Json::arrayValue);
        for (const auto& r : roles) arr.append(r.toJson());
        cb(core::Result::ok(arr));
    } catch (const std::exception& e) {
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
    }
}

drogon::AsyncTask RoleController::create(
    drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr&)> cb) {
    auto json = req->getJsonObject();
    if (!json) { cb(core::Result::fail(4001, "invalid json body")); co_return; }
    std::string err;
    auto r = dto::RoleUpsertReq::parse(*json, true, err);
    if (!r) { cb(core::Result::fail(4002, err)); co_return; }
    try {
        auto exists = co_await RbacService::instance().repo().getRoleByCode(r->code);
        if (exists) {
            cb(core::Result::fail(4090, "role code already exists",
                                  drogon::k409Conflict));
            co_return;
        }
        auto saved = co_await RbacService::instance().repo().createRole(*r);
        RbacService::instance().invalidateAll();
        cb(core::Result::ok(saved.toJson(), "created"));
    } catch (const std::exception& e) {
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
    }
}

drogon::AsyncTask RoleController::update(
    drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr&)> cb,
    int64_t id) {
    auto json = req->getJsonObject();
    if (!json) { cb(core::Result::fail(4001, "invalid json body")); co_return; }
    std::string err;
    auto r = dto::RoleUpsertReq::parse(*json, false, err);
    if (!r) { cb(core::Result::fail(4002, err)); co_return; }
    try {
        bool ok = co_await RbacService::instance().repo().updateRole(id, *r);
        if (!ok) {
            cb(core::Result::notFound("role not found"));
            co_return;
        }
        RbacService::instance().invalidateAll();
        auto saved = co_await RbacService::instance().repo().getRole(id);
        cb(core::Result::ok(saved ? saved->toJson() : Json::nullValue, "updated"));
    } catch (const std::exception& e) {
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
    }
}

drogon::AsyncTask RoleController::remove(
    drogon::HttpRequestPtr /*req*/,
    std::function<void(const drogon::HttpResponsePtr&)> cb,
    int64_t id) {
    try {
        bool ok = co_await RbacService::instance().repo().deleteRole(id);
        if (!ok) {
            cb(core::Result::notFound("role not found"));
            co_return;
        }
        RbacService::instance().invalidateAll();
        cb(core::Result::ok(Json::nullValue, "deleted"));
    } catch (const std::exception& e) {
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
    }
}

drogon::AsyncTask RoleController::getPerms(
    drogon::HttpRequestPtr /*req*/,
    std::function<void(const drogon::HttpResponsePtr&)> cb,
    int64_t id) {
    try {
        auto ids = co_await RbacService::instance().repo().getRolePermissionIds(id);
        Json::Value arr(Json::arrayValue);
        for (auto v : ids) arr.append(static_cast<Json::Int64>(v));
        Json::Value body;
        body["permission_ids"] = arr;
        cb(core::Result::ok(body));
    } catch (const std::exception& e) {
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
    }
}

drogon::AsyncTask RoleController::setPerms(
    drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr&)> cb,
    int64_t id) {
    auto json = req->getJsonObject();
    if (!json) { cb(core::Result::fail(4001, "invalid json body")); co_return; }
    std::string err;
    auto r = dto::IdListReq::parse(*json, "permission_ids", err);
    if (!r) { cb(core::Result::fail(4002, err)); co_return; }
    try {
        co_await RbacService::instance().repo().setRolePermissions(id, r->ids);
        RbacService::instance().invalidateAll();
        cb(core::Result::ok(Json::nullValue, "ok"));
    } catch (const std::exception& e) {
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
    }
}

} // namespace modules::rbac
