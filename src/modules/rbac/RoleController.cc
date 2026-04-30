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

void RoleController::list(const drogon::HttpRequestPtr&,
                          std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    try {
        auto roles = RbacService::instance().repo().listRoles();
        Json::Value arr(Json::arrayValue);
        for (const auto& r : roles) arr.append(r.toJson());
        cb(core::Result::ok(arr));
    } catch (const std::exception& e) {
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
    }
}

void RoleController::create(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    auto json = req->getJsonObject();
    if (!json) { cb(core::Result::fail(4001, "invalid json body")); return; }
    std::string err;
    auto r = dto::RoleUpsertReq::parse(*json, true, err);
    if (!r) { cb(core::Result::fail(4002, err)); return; }
    try {
        if (RbacService::instance().repo().getRoleByCode(r->code)) {
            cb(core::Result::fail(4090, "role code already exists",
                                  drogon::k409Conflict));
            return;
        }
        auto saved = RbacService::instance().repo().createRole(*r);
        RbacService::instance().invalidateAll();
        cb(core::Result::ok(saved.toJson(), "created"));
    } catch (const std::exception& e) {
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
    }
}

void RoleController::update(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                            int64_t id) {
    auto json = req->getJsonObject();
    if (!json) { cb(core::Result::fail(4001, "invalid json body")); return; }
    std::string err;
    auto r = dto::RoleUpsertReq::parse(*json, false, err);
    if (!r) { cb(core::Result::fail(4002, err)); return; }
    try {
        if (!RbacService::instance().repo().updateRole(id, *r)) {
            cb(core::Result::notFound("role not found"));
            return;
        }
        RbacService::instance().invalidateAll();
        auto saved = RbacService::instance().repo().getRole(id);
        cb(core::Result::ok(saved ? saved->toJson() : Json::nullValue, "updated"));
    } catch (const std::exception& e) {
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
    }
}

void RoleController::remove(const drogon::HttpRequestPtr&,
                            std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                            int64_t id) {
    try {
        if (!RbacService::instance().repo().deleteRole(id)) {
            cb(core::Result::notFound("role not found"));
            return;
        }
        RbacService::instance().invalidateAll();
        cb(core::Result::ok(Json::nullValue, "deleted"));
    } catch (const std::exception& e) {
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
    }
}

void RoleController::getPerms(const drogon::HttpRequestPtr&,
                              std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                              int64_t id) {
    try {
        auto ids = RbacService::instance().repo().getRolePermissionIds(id);
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

void RoleController::setPerms(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                              int64_t id) {
    auto json = req->getJsonObject();
    if (!json) { cb(core::Result::fail(4001, "invalid json body")); return; }
    std::string err;
    auto r = dto::IdListReq::parse(*json, "permission_ids", err);
    if (!r) { cb(core::Result::fail(4002, err)); return; }
    try {
        RbacService::instance().repo().setRolePermissions(id, r->ids);
        RbacService::instance().invalidateAll();
        cb(core::Result::ok(Json::nullValue, "ok"));
    } catch (const std::exception& e) {
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
    }
}

} // namespace modules::rbac
