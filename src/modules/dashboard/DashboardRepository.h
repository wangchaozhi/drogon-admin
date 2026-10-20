#pragma once

#include "dto/DashboardStatsDto.h"
#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>

namespace modules::dashboard {

class DashboardRepository {
public:
    drogon::Task<dto::DashboardStatsDto> getStats();

private:
    drogon::orm::DbClientPtr db() const;
};

} // namespace modules::dashboard
