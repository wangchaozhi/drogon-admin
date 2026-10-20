#pragma once
//
// CORS 与 API 安全响应头。Bootstrap 通过全局 advice 调用静态方法，
// Filter 接口保留给需要显式挂载的路由。
//
#include <drogon/HttpFilter.h>

namespace filters {

class CorsFilter : public drogon::HttpFilter<CorsFilter> {
public:
    static bool isPreflight(const drogon::HttpRequestPtr& req);
    static drogon::HttpResponsePtr handlePreflight(
        const drogon::HttpRequestPtr& req);
    static void applyResponseHeaders(
        const drogon::HttpRequestPtr& req,
        const drogon::HttpResponsePtr& resp);

    void doFilter(const drogon::HttpRequestPtr& req,
                  drogon::FilterCallback&& fcb,
                  drogon::FilterChainCallback&& fccb) override;
};

} // namespace filters
