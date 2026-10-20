#include "UserService.h"
#include "common/CryptoUtil.h"
#include "common/JwtUtil.h"
#include "common/TimeUtil.h"
#include "plugins/ConfigPlugin.h"
#include "modules/rbac/RbacService.h"
#include "core/Logger.h"
#include <drogon/drogon.h>
#include <string_view>

namespace modules::user {

namespace {

std::pair<std::string, int> jwtConfig() {
    auto* cfg = drogon::app().getPlugin<plugins::ConfigPlugin>();
    if (!cfg) throw std::runtime_error("ConfigPlugin unavailable");
    return {cfg->jwtSecret(), cfg->jwtExpireSeconds()};
}

std::string registrationMode() {
    auto* cfg = drogon::app().getPlugin<plugins::ConfigPlugin>();
    if (!cfg) throw std::runtime_error("ConfigPlugin unavailable");
    return cfg->registrationMode();
}

} // namespace

drogon::Task<std::optional<dto::UserDto>>
UserService::getById(int64_t id) {
    co_return co_await repo_.findById(id);
}

drogon::Task<dto::UserDto>
UserService::registerPublic(dto::CreateUserReq req) {
    const auto mode = registrationMode();
    if (mode == "disabled")
        throw RegistrationClosedError("registration is disabled");

    try {
        auto user = co_await repo_.registerWithDefaultRole(
            std::move(req.name),
            std::move(req.email),
            common::CryptoUtil::hashPassword(req.password),
            mode == "first-user");
        co_return user;
    } catch (const std::runtime_error& e) {
        if (std::string_view(e.what()) == "registration_closed")
            throw RegistrationClosedError(
                "registration is closed after the first administrator");
        throw;
    }
}

drogon::Task<dto::UserDto>
UserService::create(dto::CreateUserReq req) {
    auto hash = common::CryptoUtil::hashPassword(req.password);
    co_return co_await repo_.insertWithRole(
        std::move(req.name), std::move(req.email), std::move(hash), 2);
}

drogon::Task<std::optional<dto::UserDto>>
UserService::update(int64_t id, dto::UpdateUserReq req) {
    std::string hash;
    if (req.password) hash = common::CryptoUtil::hashPassword(*req.password);
    auto u = co_await repo_.updateById(id, req.name, req.email, hash);
    if (u) rbac::RbacService::instance().invalidateUser(id);
    co_return u;
}

drogon::Task<LoginResult>
UserService::login(dto::LoginReq req) {
    auto rec = co_await repo_.findByEmail(req.email);
    if (!rec || !common::CryptoUtil::verifyPassword(req.password, rec->passwordHash)) {
        throw LoginInvalidError("email or password incorrect");
    }
    if (common::CryptoUtil::passwordNeedsRehash(rec->passwordHash)) {
        co_await repo_.updatePasswordHash(
            rec->user.id, common::CryptoUtil::hashPassword(req.password));
        APP_LOG_INFO << "password hash upgraded uid=" << rec->user.id;
    }

    auto [secret, expireSec] = jwtConfig();
    LoginResult r;
    r.user      = rec->user;
    r.token     = common::JwtUtil::sign(rec->user.id, rec->user.email, secret, expireSec);
    r.expiresAt = common::TimeUtil::nowSec() + expireSec;

    try {
        auto view = co_await rbac::RbacService::instance().getUserView(rec->user.id);
        r.roles       = std::move(view.roles);
        r.permissions = std::move(view.perms);
        r.menus       = co_await rbac::RbacService::instance().getUserMenus(rec->user.id);
    } catch (const std::exception& e) {
        APP_LOG_ERROR << "login fetch rbac failed uid=" << rec->user.id
                      << " err=" << e.what();
    }
    co_return r;
}

} // namespace modules::user
