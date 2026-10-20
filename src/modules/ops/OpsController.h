#pragma once

#include <drogon/HttpController.h>

namespace modules::ops {

class OpsController : public drogon::HttpController<OpsController> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(
            OpsController::metrics, "/api/metrics", drogon::Get);
    METHOD_LIST_END

    void metrics(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& cb);
};

} // namespace modules::ops
