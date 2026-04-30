#pragma once
//
// UserController：承载 /api/users 与 /api/auth 相关 HTTP 路由。
// Drogon 扫描到 HttpController 派生类会自动注册，无需在 main 里 new。
//
#include "UserService.h"
#include <drogon/HttpController.h>

namespace modules::user {

class UserController : public drogon::HttpController<UserController> {
public:
    METHOD_LIST_BEGIN
        // 认证相关（公开）
        ADD_METHOD_TO(UserController::registerUser, "/api/auth/register", drogon::Post);
        ADD_METHOD_TO(UserController::login,        "/api/auth/login",    drogon::Post);

        // 受保护接口：需要合法 JWT
        ADD_METHOD_TO(UserController::me,      "/api/users/me",          drogon::Get, "filters::AuthFilter");
        ADD_METHOD_TO(UserController::getById, "/api/users/{id:\\d+}",   drogon::Get, "filters::AuthFilter");

        // 兼容旧路径，注册一个用户也可以走 /api/users POST（走注册逻辑）
        ADD_METHOD_TO(UserController::registerUser, "/api/users", drogon::Post);

        // 健康检查（公开）
        ADD_METHOD_TO(UserController::health, "/api/health", drogon::Get);
    METHOD_LIST_END

    // 注册（创建用户）
    void registerUser(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& cb);

    // 登录，返回 JWT
    void login(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& cb);

    // 当前用户信息（从 AuthFilter 写入 req attributes 中读取 userId）
    void me(const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& cb);

    void getById(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                 int64_t id);

    void health(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb);

private:
    UserService svc_;
};

} // namespace modules::user
