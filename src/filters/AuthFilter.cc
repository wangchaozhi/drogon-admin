#include "AuthFilter.h"
#include "core/Result.h"
#include "core/Logger.h"
#include "common/JwtUtil.h"
#include "plugins/ConfigPlugin.h"
#include "modules/rbac/RbacService.h"
#include <drogon/drogon.h>
#include <drogon/utils/coroutine.h>

namespace filters {

namespace {

// 异步加载 RBAC 视图后决定放行 / 拒绝。
// 参数按值传入（FilterCallback / FilterChainCallback 为 std::function，
// HttpRequestPtr 为 shared_ptr），可安全复制到协程 frame。
drogon::AsyncTask loadAndContinue(drogon::HttpRequestPtr req,
                                  int64_t userId,
                                  drogon::FilterCallback fcb,
                                  drogon::FilterChainCallback fccb) {
    try {
        auto view = co_await modules::rbac::RbacService::instance().getUserView(userId);
        auto attrs = req->attributes();
        attrs->insert("roles",          view.roles);
        attrs->insert("permissions",    view.perms);
        attrs->insert("permissionsStr", view.permsStr);
        fccb();
    } catch (const std::exception& e) {
        APP_LOG_ERROR << "load rbac view failed uid=" << userId
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

    // 基础字段同步写入 attributes
    auto attrs = req->attributes();
    attrs->insert("userId", payload->userId);
    attrs->insert("email",  payload->email);

    // RBAC 视图可能冷加载 → 异步协程里 co_await，
    // 不阻塞当前 event loop，完成后再调 fccb()。
    loadAndContinue(req, payload->userId, std::move(fcb), std::move(fccb));
}

} // namespace filters
