#pragma once
//
// 跨域 Filter：对所有挂载的路由放行常见 CORS 头。
// 生产环境建议通过 custom_config 读取白名单 origin。
//
#include <drogon/HttpFilter.h>

namespace filters {

class CorsFilter : public drogon::HttpFilter<CorsFilter> {
public:
    void doFilter(const drogon::HttpRequestPtr& req,
                  drogon::FilterCallback&& fcb,
                  drogon::FilterChainCallback&& fccb) override;
};

} // namespace filters
