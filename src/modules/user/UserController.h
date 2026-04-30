#pragma once
//
// UserController：承载 /api/users 与 /api/auth 相关 HTTP 路由。
// 协程版：所有处理函数返回 drogon::AsyncTask，参数按值传入（co_await 安全）。
//
#include "UserService.h"
#include <drogon/HttpController.h>
#include <drogon/utils/coroutine.h>

namespace modules::user {

class UserController : public drogon::HttpController<UserController> {
public:
    UserController();

    METHOD_LIST_BEGIN
        // 认证相关（公开）
        ADD_METHOD_TO(UserController::registerUser, "/api/auth/register", drogon::Post);
        ADD_METHOD_TO(UserController::login,        "/api/auth/login",    drogon::Post);

        // 受保护接口
        ADD_METHOD_TO(UserController::me,       "/api/users/me",        drogon::Get,
                      "filters::AuthFilter");
        ADD_METHOD_TO(UserController::list,     "/api/users",           drogon::Get,
                      "filters::AuthFilter", "filters::PermissionFilter");
        ADD_METHOD_TO(UserController::getById,  "/api/users/{id:\\d+}", drogon::Get,
                      "filters::AuthFilter", "filters::PermissionFilter");
        ADD_METHOD_TO(UserController::remove,   "/api/users/{id:\\d+}", drogon::Delete,
                      "filters::AuthFilter", "filters::PermissionFilter");
        ADD_METHOD_TO(UserController::getRoles, "/api/users/{id:\\d+}/roles", drogon::Get,
                      "filters::AuthFilter", "filters::PermissionFilter");
        ADD_METHOD_TO(UserController::setRoles, "/api/users/{id:\\d+}/roles", drogon::Put,
                      "filters::AuthFilter", "filters::PermissionFilter");

        // 兼容旧路径：POST /api/users 走注册逻辑（公开）
        ADD_METHOD_TO(UserController::registerUser, "/api/users", drogon::Post);

        // 健康检查（公开，不查 DB，仍为同步）
        ADD_METHOD_TO(UserController::health, "/api/health", drogon::Get);
    METHOD_LIST_END

    drogon::AsyncTask registerUser(drogon::HttpRequestPtr req,
                                   std::function<void(const drogon::HttpResponsePtr&)> cb);

    drogon::AsyncTask login(drogon::HttpRequestPtr req,
                            std::function<void(const drogon::HttpResponsePtr&)> cb);

    drogon::AsyncTask me(drogon::HttpRequestPtr req,
                         std::function<void(const drogon::HttpResponsePtr&)> cb);

    drogon::AsyncTask list(drogon::HttpRequestPtr req,
                           std::function<void(const drogon::HttpResponsePtr&)> cb);

    drogon::AsyncTask getById(drogon::HttpRequestPtr req,
                              std::function<void(const drogon::HttpResponsePtr&)> cb,
                              int64_t id);

    drogon::AsyncTask remove(drogon::HttpRequestPtr req,
                             std::function<void(const drogon::HttpResponsePtr&)> cb,
                             int64_t id);

    drogon::AsyncTask getRoles(drogon::HttpRequestPtr req,
                               std::function<void(const drogon::HttpResponsePtr&)> cb,
                               int64_t id);

    drogon::AsyncTask setRoles(drogon::HttpRequestPtr req,
                               std::function<void(const drogon::HttpResponsePtr&)> cb,
                               int64_t id);

    // 不涉及 DB，保持同步签名
    void health(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb);

private:
    UserService svc_;
};

} // namespace modules::user
