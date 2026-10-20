#include "AuditService.h"
#include "core/Logger.h"
#include "core/RequestContext.h"

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
    const auto requestId = core::requestId(request);
    try {
        const auto attributes = request->attributes();
        const int64_t actorUserId = actorOverride.value_or(
            attributes && attributes->find("userId")
                ? attributes->get<int64_t>("userId")
                : int64_t{0});

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
                      << " request_id=" << requestId
                      << " path_template=" << core::pathTemplate(request)
                      << " err=" << error.what();
    } catch (...) {
        APP_LOG_ERROR << "AUDIT WRITE FAILED"
                      << " action=" << action
                      << " target_type=" << targetType
                      << " target_id=" << targetId
                      << " request_id=" << requestId
                      << " path_template=" << core::pathTemplate(request)
                      << " err=unknown";
    }
    co_return;
}

} // namespace modules::audit
