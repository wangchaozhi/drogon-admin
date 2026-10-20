#pragma once

#include "dto/AuditLogDto.h"
#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>
#include <json/json.h>
#include <string>

namespace modules::audit {

class AuditRepository {
public:
    drogon::Task<> insert(
        int64_t actorUserId,
        std::string action,
        std::string targetType,
        std::string targetId,
        std::string requestId,
        Json::Value details);

    drogon::Task<dto::AuditLogPage> listPaged(
        int page,
        int pageSize,
        std::string action);

private:
    drogon::orm::DbClientPtr db() const;
};

} // namespace modules::audit
