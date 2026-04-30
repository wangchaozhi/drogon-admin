#pragma once
//
// 菜单管理接口：支持平铺列表 / 树 / CRUD。协程版。
//
#include <drogon/HttpController.h>
#include <drogon/utils/coroutine.h>

namespace modules::rbac {

class MenuController : public drogon::HttpController<MenuController> {
public:
    MenuController();

    METHOD_LIST_BEGIN
        ADD_METHOD_TO(MenuController::list,   "/api/menus",              drogon::Get,    "filters::AuthFilter", "filters::PermissionFilter");
        ADD_METHOD_TO(MenuController::tree,   "/api/menus/tree",         drogon::Get,    "filters::AuthFilter", "filters::PermissionFilter");
        ADD_METHOD_TO(MenuController::create, "/api/menus",              drogon::Post,   "filters::AuthFilter", "filters::PermissionFilter");
        ADD_METHOD_TO(MenuController::update, "/api/menus/{id:\\d+}",    drogon::Put,    "filters::AuthFilter", "filters::PermissionFilter");
        ADD_METHOD_TO(MenuController::remove, "/api/menus/{id:\\d+}",    drogon::Delete, "filters::AuthFilter", "filters::PermissionFilter");
    METHOD_LIST_END

    drogon::AsyncTask list  (drogon::HttpRequestPtr req,
                             std::function<void(const drogon::HttpResponsePtr&)> cb);
    drogon::AsyncTask tree  (drogon::HttpRequestPtr req,
                             std::function<void(const drogon::HttpResponsePtr&)> cb);
    drogon::AsyncTask create(drogon::HttpRequestPtr req,
                             std::function<void(const drogon::HttpResponsePtr&)> cb);
    drogon::AsyncTask update(drogon::HttpRequestPtr req,
                             std::function<void(const drogon::HttpResponsePtr&)> cb,
                             int64_t id);
    drogon::AsyncTask remove(drogon::HttpRequestPtr req,
                             std::function<void(const drogon::HttpResponsePtr&)> cb,
                             int64_t id);
};

} // namespace modules::rbac
