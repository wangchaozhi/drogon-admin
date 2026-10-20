#include "DashboardController.h"
#include "core/ErrorHandler.h"
#include "core/Result.h"
#include "filters/PermissionRegistry.h"

namespace modules::dashboard {

DashboardController::DashboardController() {
    filters::PermissionRegistry::instance().bind(
        "GET", "/api/dashboard/stats", "dashboard:stats");
}

drogon::AsyncTask DashboardController::stats(
    drogon::HttpRequestPtr request,
    std::function<void(const drogon::HttpResponsePtr&)> callback) {
    try {
        const auto stats = co_await repository_.getStats();
        callback(core::Result::ok(stats.toJson()));
    } catch (const std::exception& error) {
        callback(core::internalError(
            request, "load dashboard statistics", error, 5003));
    }
}

} // namespace modules::dashboard
