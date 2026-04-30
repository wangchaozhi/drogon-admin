#pragma once
//
// 权限过滤器：必须挂在 AuthFilter 之后。
// 根据 PermissionRegistry 的路由映射，检查 attributes 里的权限码集合。
//
#include <drogon/HttpFilter.h>

namespace filters {

class PermissionFilter : public drogon::HttpFilter<PermissionFilter> {
public:
    void doFilter(const drogon::HttpRequestPtr& req,
                  drogon::FilterCallback&& fcb,
                  drogon::FilterChainCallback&& fccb) override;
};

} // namespace filters
