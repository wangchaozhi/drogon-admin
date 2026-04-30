#pragma once
//
// Service：业务编排层（异步版）。对 Repository 的调用全部走回调，
// HTTP 线程不阻塞。真实项目会在此处加入事务、校验、跨仓库协作。
//
#include "UserRepository.h"
#include "dto/CreateUserReq.h"

namespace modules::user {

class UserService {
public:
    UserService() = default;

    void getById(int64_t id,
                 std::function<void(std::optional<dto::UserDto>)> onOk,
                 DbErrCb onErr);

    void create(const dto::CreateUserReq& req,
                std::function<void(dto::UserDto)> onOk,
                DbErrCb onErr);

private:
    UserRepository repo_;
};

} // namespace modules::user
