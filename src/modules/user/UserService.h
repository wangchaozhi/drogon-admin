#pragma once
//
// Service：业务编排层（协程版）。所有方法返回 drogon::Task<T>，
// 在协程内以 co_await 串联 Repository 与 RbacService 调用。
//
#include "UserRepository.h"
#include "dto/ChangePasswordReq.h"
#include "dto/CreateUserReq.h"
#include "dto/LoginReq.h"
#include "dto/UpdateUserReq.h"
#include "modules/rbac/dto/MenuDto.h"
#include <drogon/utils/coroutine.h>
#include <utility>

namespace modules::user {

// 登录成功结果：token + 用户信息 + 角色 / 权限 / 菜单
struct LoginResult {
    std::string  token;
    int64_t      expiresAt{0};
    dto::UserDto user;
    std::vector<std::string>             roles;
    std::vector<std::string>             permissions;
    std::vector<rbac::dto::MenuDto>      menus;
};

// 登录失败（凭证错误）使用的异常，Controller 捕获后转 401
class LoginInvalidError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class LoginThrottledError : public std::runtime_error {
public:
    LoginThrottledError(std::string message, int retryAfter)
        : std::runtime_error(std::move(message)), retryAfter_(retryAfter) {}

    int retryAfter() const { return retryAfter_; }

private:
    int retryAfter_;
};

class CurrentPasswordInvalidError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class RegistrationClosedError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class UserService {
public:
    UserService() = default;

    drogon::Task<std::optional<dto::UserDto>> getById(int64_t id);

    // 公开注册：遵循 registration_mode；首用户管理员分配在 DB 事务内完成。
    drogon::Task<dto::UserDto> registerPublic(dto::CreateUserReq req);

    // 管理员创建用户：始终绑定普通 user 角色。
    drogon::Task<dto::UserDto> create(dto::CreateUserReq req);

    // 编辑资料：name/email 必填，password 可选（空表示不改）；
    // 返回 nullopt 表示目标用户不存在
    drogon::Task<std::optional<dto::UserDto>> update(int64_t id, dto::UpdateUserReq req);

    // 登录：失败时抛 LoginInvalidError；DB 异常走底层抛出的 DrogonDbException
    drogon::Task<LoginResult> login(dto::LoginReq req);

    // 用户修改自己的密码；成功后递增 token_version，使所有现有 token 失效。
    drogon::Task<> changePassword(int64_t id, dto::ChangePasswordReq req);

    UserRepository& repo() { return repo_; }

private:
    UserRepository repo_;
};

} // namespace modules::user
