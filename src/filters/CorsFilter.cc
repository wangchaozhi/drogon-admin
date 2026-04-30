#include "CorsFilter.h"
#include <drogon/HttpResponse.h>

namespace filters {

void CorsFilter::doFilter(const drogon::HttpRequestPtr& req,
                          drogon::FilterCallback&& fcb,
                          drogon::FilterChainCallback&& fccb) {
    if (req->method() == drogon::Options) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods",
                        "GET,POST,PUT,DELETE,OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers",
                        "Content-Type,Authorization");
        resp->setStatusCode(drogon::k204NoContent);
        fcb(resp);
        return;
    }
    fccb();
}

} // namespace filters
