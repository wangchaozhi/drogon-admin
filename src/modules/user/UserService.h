#pragma once
//
// Service：业务编排层（协程版）。所有方法返回 drogon::Task<T>，
// 在协程内以 co_await 串联 Repository 与 RbacService 调用。
//
#include "UserRepository.h"
#include "dto/CreateUserReq.h"
#include "dto/LoginReq.h"
#include "dto/UpdateUserReq.h"
#include "modules/rbac/dto/MenuDto.h"
#include <drogon/utils/coroutine.h>

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

class UserService {
public:
    UserService() = default;

    drogon::Task<std::optional<dto::UserDto>> getById(int64_t id);

    // 注册：写入前对密码做哈希；成功后自动绑定角色——
    // 系统首个用户晋升 admin，超管邮箱同样 admin，其余为普通 user
    drogon::Task<dto::UserDto> create(dto::CreateUserReq req);

    // 编辑资料：name/email 必填，password 可选（空表示不改）；
    // 返回 nullopt 表示目标用户不存在
    drogon::Task<std::optional<dto::UserDto>> update(int64_t id, dto::UpdateUserReq req);

    // 登录：失败时抛 LoginInvalidError；DB 异常走底层抛出的 DrogonDbException
    drogon::Task<LoginResult> login(dto::LoginReq req);

    UserRepository& repo() { return repo_; }

private:
    UserRepository repo_;
};

} // namespace modules::user
