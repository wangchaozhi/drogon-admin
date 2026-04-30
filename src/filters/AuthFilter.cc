#include "AuthFilter.h"
#include "core/Result.h"
#include "core/Logger.h"
#include "common/JwtUtil.h"
#include "plugins/ConfigPlugin.h"
#include "modules/rbac/RbacService.h"
#include <drogon/drogon.h>

namespace filters {

void AuthFilter::doFilter(const drogon::HttpRequestPtr& req,
                          drogon::FilterCallback&& fcb,
                          drogon::FilterChainCallback&& fccb) {
    const auto& auth = req->getHeader("Authorization");
    if (auth.rfind("Bearer ", 0) != 0 || auth.size() <= 7) {
        APP_LOG_WARN << "auth rejected (no bearer): " << req->getPath();
        fcb(core::Result::unauthorized("missing or invalid token"));
        return;
    }
    std::string token = auth.substr(7);

    auto* cfg = drogon::app().getPlugin<plugins::ConfigPlugin>();
    std::string secret = cfg ? cfg->jwtSecret() : std::string("change-me");

    std::string err;
    auto payload = common::JwtUtil::verify(token, secret, err);
    if (!payload) {
        APP_LOG_WARN << "auth rejected: " << err << " path=" << req->getPath();
        fcb(core::Result::unauthorized("invalid token: " + err));
        return;
    }

    // 基础字段
    auto attrs = req->attributes();
    attrs->insert("userId", payload->userId);
    attrs->insert("email",  payload->email);

    // RBAC 权限视图（带缓存，首次冷加载才打 DB）
    try {
        auto view = modules::rbac::RbacService::instance().getUserView(payload->userId);
        attrs->insert("roles",          view.roles);
        attrs->insert("permissions",    view.perms);
        attrs->insert("permissionsStr", view.permsStr);
    } catch (const std::exception& e) {
        APP_LOG_ERROR << "load rbac view failed uid=" << payload->userId
                      << " err=" << e.what();
        fcb(core::Result::fail(5004, "load permissions failed",
                               drogon::k500InternalServerError));
        return;
    }

    fccb();
}

} // namespace filters
