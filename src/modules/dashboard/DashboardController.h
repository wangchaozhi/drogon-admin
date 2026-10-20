#pragma once

#include "DashboardRepository.h"
#include <drogon/HttpController.h>
#include <drogon/utils/coroutine.h>

namespace modules::dashboard {

class DashboardController
    : public drogon::HttpController<DashboardController> {
public:
    DashboardController();

    METHOD_LIST_BEGIN
        ADD_METHOD_TO(
            DashboardController::stats,
            "/api/dashboard/stats",
            drogon::Get,
            "filters::AuthFilter",
            "filters::PermissionFilter");
    METHOD_LIST_END

    drogon::AsyncTask stats(
        drogon::HttpRequestPtr request,
        std::function<void(const drogon::HttpResponsePtr&)> callback);

private:
    DashboardRepository repository_;
};

} // namespace modules::dashboard
