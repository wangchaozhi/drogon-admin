#include "PermissionController.h"
#include "RbacService.h"
#include "core/Result.h"
#include "filters/PermissionRegistry.h"

namespace modules::rbac {

PermissionController::PermissionController() {
    // 查看权限列表用作角色管理页的分配树；权限码共用 role:view
    filters::PermissionRegistry::instance().bind(
        "GET", "/api/permissions", "role:view");
}

drogon::AsyncTask PermissionController::list(
    drogon::HttpRequestPtr /*req*/,
    std::function<void(const drogon::HttpResponsePtr&)> cb) {
    try {
        auto perms = co_await RbacService::instance().repo().listPermissions();
        Json::Value arr(Json::arrayValue);
        for (const auto& p : perms) arr.append(p.toJson());
        cb(core::Result::ok(arr));
    } catch (const std::exception& e) {
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
    }
}

} // namespace modules::rbac
