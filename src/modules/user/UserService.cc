#include "UserService.h"
#include "common/CryptoUtil.h"
#include "common/JwtUtil.h"
#include "common/TimeUtil.h"
#include "plugins/ConfigPlugin.h"
#include "modules/rbac/RbacService.h"
#include "core/Logger.h"
#include <drogon/drogon.h>
#include <algorithm>
#include <string_view>

namespace modules::user {

namespace {

struct AuthConfig {
    std::string secret;
    int expireSeconds;
    int maxFailures;
    int lockSeconds;
};

AuthConfig authConfig() {
    auto* cfg = drogon::app().getPlugin<plugins::ConfigPlugin>();
    if (!cfg) throw std::runtime_error("ConfigPlugin unavailable");
    return {
        cfg->jwtSecret(),
        cfg->jwtExpireSeconds(),
        cfg->loginMaxFailures(),
        cfg->loginLockSeconds(),
    };
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
    auto u = co_await repo_.updateById(
        id, req.name, req.email, hash, req.disabled);
    if (u) rbac::RbacService::instance().invalidateUser(id);
    co_return u;
}

drogon::Task<LoginResult>
UserService::login(dto::LoginReq req) {
    const auto cfg = authConfig();
    const int64_t now = common::TimeUtil::nowSec();
    auto rec = co_await repo_.findByEmail(req.email);
    if (rec && rec->lockedUntil > now) {
        throw LoginThrottledError(
            "too many login attempts; try again later",
            static_cast<int>(std::max<int64_t>(1, rec->lockedUntil - now)));
    }
    if (!rec) {
        common::CryptoUtil::performDummyPasswordCheck(req.password);
        throw LoginInvalidError("email or password incorrect");
    }
    if (!common::CryptoUtil::verifyPassword(
            req.password, rec->passwordHash)) {
        const auto state = co_await repo_.recordLoginFailure(
            rec->user.id, cfg.maxFailures, cfg.lockSeconds);
        if (state.lockedUntil > now) {
            throw LoginThrottledError(
                "too many login attempts; try again later",
                static_cast<int>(
                    std::max<int64_t>(1, state.lockedUntil - now)));
        }
        throw LoginInvalidError("email or password incorrect");
    }
    if (rec->user.disabled)
        throw LoginInvalidError("email or password incorrect");

    co_await repo_.resetLoginFailures(rec->user.id);
    if (common::CryptoUtil::passwordNeedsRehash(rec->passwordHash)) {
        co_await repo_.updatePasswordHash(
            rec->user.id, common::CryptoUtil::hashPassword(req.password));
        APP_LOG_INFO << "password hash upgraded uid=" << rec->user.id;
    }

    LoginResult r;
    r.user      = rec->user;
    r.token     = common::JwtUtil::sign(
        rec->user.id,
        rec->user.email,
        rec->tokenVersion,
        cfg.secret,
        cfg.expireSeconds);
    r.expiresAt = common::TimeUtil::nowSec() + cfg.expireSeconds;

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

drogon::Task<> UserService::changePassword(
    int64_t id,
    dto::ChangePasswordReq req) {
    auto currentHash = co_await repo_.findPasswordHash(id);
    if (!currentHash
        || !common::CryptoUtil::verifyPassword(
            req.currentPassword, *currentHash)) {
        throw CurrentPasswordInvalidError("current password is incorrect");
    }

    auto newHash =
        common::CryptoUtil::hashPassword(req.newPassword);
    const bool updated = co_await repo_.replacePasswordHash(
        id, std::move(*currentHash), std::move(newHash));
    if (!updated)
        throw CurrentPasswordInvalidError(
            "password changed concurrently; please try again");
    APP_LOG_INFO << "password changed uid=" << id;
    co_return;
}

} // namespace modules::user
