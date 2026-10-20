#include "OpsController.h"
#include "core/Metrics.h"

namespace modules::ops {

void OpsController::metrics(
    const drogon::HttpRequestPtr& /*req*/,
    std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    auto response = drogon::HttpResponse::newHttpResponse();
    response->setStatusCode(drogon::k200OK);
    response->setContentTypeString(
        "text/plain; version=0.0.4; charset=utf-8");
    response->addHeader("Cache-Control", "no-store");
    response->setBody(core::Metrics::instance().renderPrometheus());
    cb(response);
}

} // namespace modules::ops
