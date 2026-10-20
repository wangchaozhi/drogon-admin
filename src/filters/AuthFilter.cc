#include "AuthFilter.h"
#include "core/RequestContext.h"
#include "core/Result.h"
#include "core/Logger.h"
#include "common/JwtUtil.h"
#include "plugins/ConfigPlugin.h"
#include "modules/rbac/RbacService.h"
#include "modules/user/UserRepository.h"
#include <drogon/drogon.h>
#include <drogon/utils/coroutine.h>

namespace filters {

namespace {

// 异步加载 RBAC 视图后决定放行 / 拒绝。
// 参数按值传入（FilterCallback / FilterChainCallback 为 std::function，
// HttpRequestPtr 为 shared_ptr），可安全复制到协程 frame。
drogon::AsyncTask loadAndContinue(drogon::HttpRequestPtr req,
                                  int64_t userId,
                                  std::string email,
                                  int64_t tokenVersion,
                                  drogon::FilterCallback fcb,
                                  drogon::FilterChainCallback fccb) {
    try {
        modules::user::UserRepository users;
        const auto state = co_await users.findAuthState(userId);
        if (!state || state->disabled
            || state->tokenVersion != tokenVersion) {
            fcb(core::Result::unauthorized("token has been revoked"));
            co_return;
        }

        auto view = co_await modules::rbac::RbacService::instance().getUserView(userId);
        auto attrs = req->attributes();
        attrs->insert("userId",         userId);
        attrs->insert("email",          std::move(email));
        attrs->insert("roles",          view.roles);
        attrs->insert("permissions",    view.perms);
        attrs->insert("permissionsStr", view.permsStr);
        fccb();
    } catch (const std::exception& e) {
        APP_LOG_ERROR << "load rbac view failed uid=" << userId
                      << " request_id=" << core::requestId(req)
                      << " path_template=" << core::pathTemplate(req)
                      << " err=" << e.what();
        fcb(core::Result::fail(5004, "load permissions failed",
                               drogon::k500InternalServerError));
    }
}

} // namespace

void AuthFilter::doFilter(const drogon::HttpRequestPtr& req,
                          drogon::FilterCallback&& fcb,
                          drogon::FilterChainCallback&& fccb) {
    const auto& auth = req->getHeader("Authorization");
    if (auth.rfind("Bearer ", 0) != 0 || auth.size() <= 7) {
        APP_LOG_WARN << "auth rejected reason=missing_bearer"
                     << " request_id=" << core::requestId(req)
                     << " path_template=" << core::pathTemplate(req);
        fcb(core::Result::unauthorized("missing or invalid token"));
        return;
    }
    std::string token = auth.substr(7);

    auto* cfg = drogon::app().getPlugin<plugins::ConfigPlugin>();
    if (!cfg) {
        APP_LOG_ERROR << "auth rejected reason=config_unavailable"
                      << " request_id=" << core::requestId(req)
                      << " path_template=" << core::pathTemplate(req);
        fcb(core::Result::fail(5005, "authentication unavailable",
                               drogon::k500InternalServerError));
        return;
    }

    std::string err;
    auto payload = common::JwtUtil::verify(token, cfg->jwtSecret(), err);
    if (!payload) {
        APP_LOG_WARN << "auth rejected reason=" << err
                     << " request_id=" << core::requestId(req)
                     << " path_template=" << core::pathTemplate(req);
        fcb(core::Result::unauthorized("invalid token: " + err));
        return;
    }

    // 账号状态与 RBAC 视图都需要异步查询，验证后再写入 attributes 并放行。
    loadAndContinue(req,
                    payload->userId,
                    std::move(payload->email),
                    payload->tokenVersion,
                    std::move(fcb),
                    std::move(fccb));
}

} // namespace filters
