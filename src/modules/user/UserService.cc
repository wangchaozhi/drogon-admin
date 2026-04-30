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
// 统一从 ConfigPlugin 读取 JWT 配置
std::pair<std::string, int> jwtConfig() {
    auto* cfg = drogon::app().getPlugin<plugins::ConfigPlugin>();
    if (!cfg) return {"change-me", 7200};
    return {cfg->jwtSecret(), cfg->jwtExpireSeconds()};
}

std::string superAdminEmail() {
    auto* cfg = drogon::app().getPlugin<plugins::ConfigPlugin>();
    return cfg ? cfg->superAdminEmail() : std::string("admin@c_web.local");
}

// 注册时刻 users 表是否"此前为空"：
// 当前这条新记录已经写入，所以总数 <= 1 即表示此前没有任何用户
bool isFirstRegisteredUser() {
    try {
        auto db = drogon::app().getDbClient();
        if (!db) return false;  // 拿不到 DbClient 时保守处理，不提升权限
        auto r = db->execSqlSync("SELECT COUNT(*) AS c FROM users");
        if (r.empty()) return false;
        return r[0]["c"].as<int64_t>() <= 1;
    } catch (const std::exception& e) {
        APP_LOG_ERROR << "isFirstRegisteredUser check failed: " << e.what();
        return false;  // 查询异常时不提升，避免误授权
    }
}

// 注册成功后自动绑定角色：
// 1) 注册时 users 表此前为空 → 当前用户即首个管理员 (id=1)
// 2) 配置中的超管邮箱 → admin (id=1)
// 3) 其他 → 普通用户 (id=2)
void assignDefaultRole(int64_t userId, const std::string& email) {
    auto& svc = rbac::RbacService::instance();
    try {
        bool isFirstAdmin = isFirstRegisteredUser();
        int64_t roleId = 2;
        if (isFirstAdmin) {
            roleId = 1;
            APP_LOG_INFO << "first registered user promoted to admin uid=" << userId
                         << " email=" << email;
        } else if (email == superAdminEmail()) {
            roleId = 1;
        }
        auto current = svc.repo().getUserRoleIds(userId);
        if (std::find(current.begin(), current.end(), roleId) == current.end()) {
            current.push_back(roleId);
            svc.repo().setUserRoles(userId, current);
            svc.invalidateUser(userId);
        }
    } catch (const std::exception& e) {
        APP_LOG_ERROR << "assignDefaultRole failed uid=" << userId
                      << " err=" << e.what();
    }
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
    repo_.insert(req.name, req.email, hash,
        [onOk = std::move(onOk)](dto::UserDto u) {
            assignDefaultRole(u.id, u.email);
            onOk(std::move(u));
        },
        std::move(onErr));
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

            // 附加 RBAC 视图（带缓存）
            try {
                auto view = rbac::RbacService::instance().getUserView(rec->user.id);
                r.roles       = std::move(view.roles);
                r.permissions = std::move(view.perms);
                r.menus       = rbac::RbacService::instance().getUserMenus(rec->user.id);
            } catch (const std::exception& e) {
                APP_LOG_ERROR << "login fetch rbac failed uid=" << rec->user.id
                              << " err=" << e.what();
            }
            onOk(std::move(r));
        },
        std::move(onErr));
}

} // namespace modules::user
