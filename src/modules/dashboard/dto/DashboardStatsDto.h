#pragma once

#include <json/json.h>
#include <cstdint>

namespace modules::dashboard::dto {

struct DashboardStatsDto {
    int64_t totalUsers{0};
    int64_t enabledUsers{0};
    int64_t disabledUsers{0};
    int64_t lockedUsers{0};
    int64_t totalRoles{0};
    int64_t visibleMenus{0};
    int64_t auditEvents24h{0};
    int64_t generatedAt{0};

    Json::Value toJson() const {
        Json::Value value;
        value["total_users"] = static_cast<Json::Int64>(totalUsers);
        value["enabled_users"] = static_cast<Json::Int64>(enabledUsers);
        value["disabled_users"] = static_cast<Json::Int64>(disabledUsers);
        value["locked_users"] = static_cast<Json::Int64>(lockedUsers);
        value["total_roles"] = static_cast<Json::Int64>(totalRoles);
        value["visible_menus"] = static_cast<Json::Int64>(visibleMenus);
        value["audit_events_24h"] =
            static_cast<Json::Int64>(auditEvents24h);
        value["generated_at"] = static_cast<Json::Int64>(generatedAt);
        return value;
    }
};

} // namespace modules::dashboard::dto
