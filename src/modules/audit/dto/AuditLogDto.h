#pragma once

#include <json/json.h>
#include <cstdint>
#include <string>
#include <vector>

namespace modules::audit::dto {

struct AuditLogDto {
    int64_t id{0};
    int64_t actorUserId{0};
    std::string action;
    std::string targetType;
    std::string targetId;
    std::string requestId;
    Json::Value details{Json::objectValue};
    int64_t createdAt{0};

    Json::Value toJson() const {
        Json::Value value;
        value["id"] = static_cast<Json::Int64>(id);
        value["actor_user_id"] =
            static_cast<Json::Int64>(actorUserId);
        value["action"] = action;
        value["target_type"] = targetType;
        value["target_id"] = targetId;
        value["request_id"] = requestId;
        value["details"] = details;
        value["created_at"] = static_cast<Json::Int64>(createdAt);
        return value;
    }
};

struct AuditLogPage {
    std::vector<AuditLogDto> items;
    int64_t total{0};
};

struct AuditLogQuery {
    int page{1};
    int pageSize{20};
    std::string action;
    int64_t actorUserId{0};
    std::string targetType;
    std::string targetId;
    std::string requestId;
};

} // namespace modules::audit::dto
