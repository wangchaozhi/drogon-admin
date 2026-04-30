#pragma once
//
// 权限列表查询接口（权限通过菜单维护，不直接增删）。
//
#include <drogon/HttpController.h>

namespace modules::rbac {

class PermissionController : public drogon::HttpController<PermissionController> {
public:
    PermissionController();

    METHOD_LIST_BEGIN
        ADD_METHOD_TO(PermissionController::list, "/api/permissions", drogon::Get,
                      "filters::AuthFilter", "filters::PermissionFilter");
    METHOD_LIST_END

    void list(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& cb);
};

} // namespace modules::rbac
