#pragma once

#include <drogon/HttpController.h>
#include <drogon/utils/coroutine.h>

namespace modules::audit {

class AuditController : public drogon::HttpController<AuditController> {
public:
    AuditController();

    METHOD_LIST_BEGIN
        ADD_METHOD_TO(
            AuditController::list,
            "/api/audit-logs",
            drogon::Get,
            "filters::AuthFilter",
            "filters::PermissionFilter");
    METHOD_LIST_END

    drogon::AsyncTask list(
        drogon::HttpRequestPtr request,
        std::function<void(const drogon::HttpResponsePtr&)> callback);
};

} // namespace modules::audit
