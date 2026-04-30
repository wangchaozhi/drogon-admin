#include "UserService.h"
#include "common/CryptoUtil.h"
#include "common/JwtUtil.h"
#include "common/TimeUtil.h"
#include "plugins/ConfigPlugin.h"
#include "modules/rbac/RbacService.h"
#include "core/Logger.h"
#include <drogon/drogon.h>
#include <algorithm>

namespace modules::user {

namespace {

std::pair<std::string, int> jwtConfig() {
    auto* cfg = drogon::app().getPlugin<plugins::ConfigPlugin>();
    if (!cfg) return {"change-me", 7200};
    return {cfg->jwtSecret(), cfg->jwtExpireSeconds()};
}

std::string superAdminEmail() {
    auto* cfg = drogon::app().getPlugin<plugins::ConfigPlugin>();
    return cfg ? cfg->superAdminEmail() : std::string("admin@drogon-admin.local");
}

// 注册成功后自动绑定角色（协程版）：
// 1) 系统尚未存在任何 admin → 当前用户即首个管理员 (id=1)
// 2) 配置中的超管邮箱 → admin (id=1)
// 3) 其他 → 普通用户 (id=2)
drogon::Task<> assignDefaultRole(UserRepository& repo,
                                 int64_t userId,
                                 std::string email) {
    auto& svc = rbac::RbacService::instance();
    try {
        bool hasAdmin = co_await repo.hasAnyAdmin();
        int64_t roleId = 2;
        if (!hasAdmin) {
            roleId = 1;
            APP_LOG_INFO << "first registered user promoted to admin uid=" << userId
                         << " email=" << email;
        } else if (email == superAdminEmail()) {
            roleId = 1;
        }
        auto current = co_await svc.repo().getUserRoleIds(userId);
        if (std::find(current.begin(), current.end(), roleId) == current.end()) {
            current.push_back(roleId);
            co_await svc.repo().setUserRoles(userId, current);
            svc.invalidateUser(userId);
        }
    } catch (const std::exception& e) {
        APP_LOG_ERROR << "assignDefaultRole failed uid=" << userId
                      << " err=" << e.what();
    }
    co_return;
}

} // namespace

drogon::Task<std::optional<dto::UserDto>>
UserService::getById(int64_t id) {
    co_return co_await repo_.findById(id);
}

drogon::Task<dto::UserDto>
UserService::create(dto::CreateUserReq req) {
    auto hash = common::CryptoUtil::hashPassword(req.password);
    auto u = co_await repo_.insert(req.name, req.email, hash);
    co_await assignDefaultRole(repo_, u.id, u.email);
    co_return u;
}

drogon::Task<LoginResult>
UserService::login(dto::LoginReq req) {
    auto rec = co_await repo_.findByEmail(req.email);
    if (!rec || !common::CryptoUtil::verifyPassword(req.password, rec->passwordHash)) {
        throw LoginInvalidError("email or password incorrect");
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
