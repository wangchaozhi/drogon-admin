#pragma once
//
// 角色管理接口。所有接口均受 AuthFilter + PermissionFilter 保护，
// 具体权限码通过 PermissionRegistry 在 ctor 中注册。
//
#include <drogon/HttpController.h>

namespace modules::rbac {

class RoleController : public drogon::HttpController<RoleController> {
public:
    RoleController();

    METHOD_LIST_BEGIN
        ADD_METHOD_TO(RoleController::list,      "/api/roles",        drogon::Get,    "filters::AuthFilter", "filters::PermissionFilter");
        ADD_METHOD_TO(RoleController::create,    "/api/roles",        drogon::Post,   "filters::AuthFilter", "filters::PermissionFilter");
        ADD_METHOD_TO(RoleController::update,    "/api/roles/{id:\\d+}", drogon::Put,    "filters::AuthFilter", "filters::PermissionFilter");
        ADD_METHOD_TO(RoleController::remove,    "/api/roles/{id:\\d+}", drogon::Delete, "filters::AuthFilter", "filters::PermissionFilter");
        ADD_METHOD_TO(RoleController::getPerms,  "/api/roles/{id:\\d+}/permissions", drogon::Get, "filters::AuthFilter", "filters::PermissionFilter");
        ADD_METHOD_TO(RoleController::setPerms,  "/api/roles/{id:\\d+}/permissions", drogon::Put, "filters::AuthFilter", "filters::PermissionFilter");
    METHOD_LIST_END

    void list    (const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& cb);
    void create  (const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& cb);
    void update  (const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                  int64_t id);
    void remove  (const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                  int64_t id);
    void getPerms(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                  int64_t id);
    void setPerms(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                  int64_t id);
};

} // namespace modules::rbac
