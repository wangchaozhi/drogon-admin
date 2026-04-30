#include "PermissionFilter.h"
#include "PermissionRegistry.h"
#include "core/Result.h"
#include "core/Logger.h"

namespace filters {

static std::string methodToStr(drogon::HttpMethod m) {
    switch (m) {
        case drogon::Get:     return "GET";
        case drogon::Post:    return "POST";
        case drogon::Put:     return "PUT";
        case drogon::Delete:  return "DELETE";
        case drogon::Patch:   return "PATCH";
        case drogon::Head:    return "HEAD";
        case drogon::Options: return "OPTIONS";
        default:              return "UNKNOWN";
    }
}

void PermissionFilter::doFilter(const drogon::HttpRequestPtr& req,
                                drogon::FilterCallback&& fcb,
                                drogon::FilterChainCallback&& fccb) {
    auto attrs = req->attributes();
    if (!attrs || !attrs->find("userId")) {
        fcb(core::Result::unauthorized("missing auth context"));
        return;
    }

    std::string method = methodToStr(req->getMethod());
    std::string path   = req->getPath();

    auto need = PermissionRegistry::instance().lookup(method, path);
    if (!need) {
        fccb();  // 路由未声明所需权限 → 放行（仅要求已登录）
        return;
    }

    // permissionsStr 形如 "|dashboard:view|user:view|..."
    std::string perms = attrs->find("permissionsStr")
                        ? attrs->get<std::string>("permissionsStr")
                        : std::string();
    std::string needle = "|" + *need + "|";
    if (perms.find(needle) == std::string::npos) {
        APP_LOG_WARN << "perm denied user=" << attrs->get<int64_t>("userId")
                     << " need=" << *need << " path=" << path;
        fcb(core::Result::fail(4030, "no permission: " + *need,
                               drogon::k403Forbidden));
        return;
    }
    fccb();
}

} // namespace filters
