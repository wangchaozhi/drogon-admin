#pragma once
//
// UserController：承载 /api/users 相关 HTTP 路由。
// Drogon 扫描到 HttpController 派生类会自动注册，无需在 main 里 new。
//
#include "UserService.h"
#include <drogon/HttpController.h>

namespace modules::user {

class UserController : public drogon::HttpController<UserController> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(UserController::getById, "/api/users/{id:\\d+}", drogon::Get);
        ADD_METHOD_TO(UserController::create,  "/api/users",           drogon::Post);
        ADD_METHOD_TO(UserController::health,  "/api/health",          drogon::Get);
    METHOD_LIST_END

    void getById(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                 int64_t id);

    void create(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb);

    void health(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& cb);

private:
    UserService svc_;
};

} // namespace modules::user
