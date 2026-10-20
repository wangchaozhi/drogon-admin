#include "DashboardRepository.h"
#include "common/TimeUtil.h"
#include <drogon/drogon.h>
#include <stdexcept>

namespace modules::dashboard {

drogon::orm::DbClientPtr DashboardRepository::db() const {
    return drogon::app().getDbClient();
}

drogon::Task<dto::DashboardStatsDto> DashboardRepository::getStats() {
    const auto now = common::TimeUtil::nowSec();
    const auto since = now - 24 * 60 * 60;
    const auto rows = co_await db()->execSqlCoro(
        "SELECT "
        "(SELECT COUNT(*) FROM users) AS total_users,"
        "(SELECT COUNT(*) FROM users WHERE disabled=0) AS enabled_users,"
        "(SELECT COUNT(*) FROM users WHERE disabled<>0) AS disabled_users,"
        "(SELECT COUNT(*) FROM users "
        " WHERE disabled=0 AND locked_until>?) AS locked_users,"
        "(SELECT COUNT(*) FROM roles) AS total_roles,"
        "(SELECT COUNT(*) FROM menus WHERE visible=1) AS visible_menus,"
        "(SELECT COUNT(*) FROM audit_logs "
        " WHERE created_at>=?) AS audit_events_24h",
        now,
        since);
    if (rows.empty())
        throw std::runtime_error("dashboard statistics unavailable");

    dto::DashboardStatsDto stats;
    stats.totalUsers = rows[0]["total_users"].as<int64_t>();
    stats.enabledUsers = rows[0]["enabled_users"].as<int64_t>();
    stats.disabledUsers = rows[0]["disabled_users"].as<int64_t>();
    stats.lockedUsers = rows[0]["locked_users"].as<int64_t>();
    stats.totalRoles = rows[0]["total_roles"].as<int64_t>();
    stats.visibleMenus = rows[0]["visible_menus"].as<int64_t>();
    stats.auditEvents24h = rows[0]["audit_events_24h"].as<int64_t>();
    stats.generatedAt = now;
    co_return stats;
}

} // namespace modules::dashboard
