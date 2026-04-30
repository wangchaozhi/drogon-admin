#include "UserService.h"

namespace modules::user {

void UserService::getById(int64_t id,
                          std::function<void(std::optional<dto::UserDto>)> onOk,
                          DbErrCb onErr) {
    repo_.findById(id, std::move(onOk), std::move(onErr));
}

void UserService::create(const dto::CreateUserReq& req,
                         std::function<void(dto::UserDto)> onOk,
                         DbErrCb onErr) {
    repo_.insert(req.name, req.email, std::move(onOk), std::move(onErr));
}

} // namespace modules::user
