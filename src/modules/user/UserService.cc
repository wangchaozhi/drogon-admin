#include "UserService.h"
#include "common/CryptoUtil.h"
#include "common/JwtUtil.h"
#include "common/TimeUtil.h"
#include "plugins/ConfigPlugin.h"
#include <drogon/drogon.h>

namespace modules::user {

namespace {
// 统一从 ConfigPlugin 读取 JWT 配置
std::pair<std::string, int> jwtConfig() {
    auto* cfg = drogon::app().getPlugin<plugins::ConfigPlugin>();
    if (!cfg) return {"change-me", 7200};
    return {cfg->jwtSecret(), cfg->jwtExpireSeconds()};
}
} // namespace

void UserService::getById(int64_t id,
                          std::function<void(std::optional<dto::UserDto>)> onOk,
                          DbErrCb onErr) {
    repo_.findById(id, std::move(onOk), std::move(onErr));
}

void UserService::create(const dto::CreateUserReq& req,
                         std::function<void(dto::UserDto)> onOk,
                         DbErrCb onErr) {
    auto hash = common::CryptoUtil::hashPassword(req.password);
    repo_.insert(req.name, req.email, hash, std::move(onOk), std::move(onErr));
}

void UserService::login(const dto::LoginReq& req,
                        std::function<void(LoginResult)> onOk,
                        std::function<void(const std::string&)> onInvalid,
                        DbErrCb onErr) {
    repo_.findByEmail(
        req.email,
        [password = req.password,
         onOk      = std::move(onOk),
         onInvalid = std::move(onInvalid)](std::optional<UserRecord> rec) {
            if (!rec) { onInvalid("email or password incorrect"); return; }
            if (!common::CryptoUtil::verifyPassword(password, rec->passwordHash)) {
                onInvalid("email or password incorrect");
                return;
            }
            auto [secret, expireSec] = jwtConfig();
            LoginResult r;
            r.user      = rec->user;
            r.token     = common::JwtUtil::sign(
                rec->user.id, rec->user.email, secret, expireSec);
            r.expiresAt = common::TimeUtil::nowSec() + expireSec;
            onOk(std::move(r));
        },
        std::move(onErr));
}

} // namespace modules::user
