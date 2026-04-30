#include "AuthFilter.h"
#include "core/Result.h"
#include "core/Logger.h"
#include "common/JwtUtil.h"
#include "plugins/ConfigPlugin.h"
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

    // 把解析结果放进 attributes，供 Controller 读取
    auto attrs = req->attributes();
    attrs->insert("userId", payload->userId);
    attrs->insert("email",  payload->email);

    fccb();
}

} // namespace filters
