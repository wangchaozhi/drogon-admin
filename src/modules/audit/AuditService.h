#pragma once

#include "AuditRepository.h"
#include <drogon/HttpRequest.h>
#include <drogon/utils/coroutine.h>
#include <json/json.h>
#include <optional>
#include <string>

namespace modules::audit {

class AuditService {
public:
    static AuditService& instance();

    // 审计采用 best-effort 语义：任何写入异常仅告警，不影响主业务响应。
    drogon::Task<> record(
        const drogon::HttpRequestPtr& request,
        std::string action,
        std::string targetType,
        std::string targetId,
        Json::Value details = Json::Value(Json::objectValue),
        std::optional<int64_t> actorOverride = std::nullopt);

    AuditRepository& repo() { return repository_; }

private:
    AuditRepository repository_;
};

} // namespace modules::audit
