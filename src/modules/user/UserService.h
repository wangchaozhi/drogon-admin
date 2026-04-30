#pragma once
//
// Service：业务编排层（异步版）。对 Repository 的调用全部走回调，
// HTTP 线程不阻塞。真实项目会在此处加入事务、校验、跨仓库协作。
//
#include "UserRepository.h"
#include "dto/CreateUserReq.h"
#include "dto/LoginReq.h"

namespace modules::user {

// 登录成功结果：token + 用户信息
struct LoginResult {
    std::string  token;
    int64_t      expiresAt{0};
    dto::UserDto user;
};

class UserService {
public:
    UserService() = default;

    void getById(int64_t id,
                 std::function<void(std::optional<dto::UserDto>)> onOk,
                 DbErrCb onErr);

    // 注册：写入前对密码做哈希
    void create(const dto::CreateUserReq& req,
                std::function<void(dto::UserDto)> onOk,
                DbErrCb onErr);

    // 登录：成功时 onOk 返回 LoginResult；用户不存在 / 密码错误时走 onInvalid
    void login(const dto::LoginReq& req,
               std::function<void(LoginResult)> onOk,
               std::function<void(const std::string& /*msg*/)> onInvalid,
               DbErrCb onErr);

private:
    UserRepository repo_;
};

} // namespace modules::user
