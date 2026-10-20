#include "AuditService.h"
#include "core/Logger.h"
#include <drogon/utils/Utilities.h>

namespace modules::audit {

AuditService& AuditService::instance() {
    static AuditService service;
    return service;
}

drogon::Task<> AuditService::record(
    const drogon::HttpRequestPtr& request,
    std::string action,
    std::string targetType,
    std::string targetId,
    Json::Value details,
    std::optional<int64_t> actorOverride) {
    try {
        const auto attributes = request->attributes();
        const int64_t actorUserId = actorOverride.value_or(
            attributes && attributes->find("userId")
                ? attributes->get<int64_t>("userId")
                : int64_t{0});
        const std::string requestId =
            attributes && attributes->find("requestId")
                ? attributes->get<std::string>("requestId")
                : drogon::utils::genRandomString(24);

        if (actorUserId <= 0)
            throw std::runtime_error("audit actor is unavailable");

        co_await repository_.insert(
            actorUserId,
            action,
            targetType,
            targetId,
            requestId,
            std::move(details));
    } catch (const std::exception& error) {
        APP_LOG_ERROR << "AUDIT WRITE FAILED"
                      << " action=" << action
                      << " target_type=" << targetType
                      << " target_id=" << targetId
                      << " err=" << error.what();
    } catch (...) {
        APP_LOG_ERROR << "AUDIT WRITE FAILED"
                      << " action=" << action
                      << " target_type=" << targetType
                      << " target_id=" << targetId
                      << " err=unknown";
    }
    co_return;
}

} // namespace modules::audit
