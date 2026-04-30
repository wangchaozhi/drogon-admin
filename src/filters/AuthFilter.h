#pragma once
//
// 简单鉴权 Filter：检查请求头 Authorization: Bearer <token>
// 在 Controller 路由中通过 ADD_METHOD_TO(..., "Filters/AuthFilter") 挂载。
// 实际业务应替换为 JWT 校验，这里只做格式检查作为示例。
//
#include <drogon/HttpFilter.h>

namespace filters {

class AuthFilter : public drogon::HttpFilter<AuthFilter> {
public:
    void doFilter(const drogon::HttpRequestPtr& req,
                  drogon::FilterCallback&& fcb,
                  drogon::FilterChainCallback&& fccb) override;
};

} // namespace filters
