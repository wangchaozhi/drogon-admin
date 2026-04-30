#include "AuthFilter.h"
#include "core/Result.h"
#include "core/Logger.h"

namespace filters {

void AuthFilter::doFilter(const drogon::HttpRequestPtr& req,
                          drogon::FilterCallback&& fcb,
                          drogon::FilterChainCallback&& fccb) {
    const auto& auth = req->getHeader("Authorization");
    if (auth.rfind("Bearer ", 0) != 0 || auth.size() <= 7) {
        APP_LOG_WARN << "auth rejected: " << req->getPath();
        fcb(core::Result::unauthorized("missing or invalid token"));
        return;
    }
    // TODO: 替换为真正的 JWT 校验，并把解析出的 userId 放到 req attributes
    fccb();
}

} // namespace filters
