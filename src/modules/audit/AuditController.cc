#include "AuditController.h"
#include "AuditService.h"
#include "core/ErrorHandler.h"
#include "core/Result.h"
#include "filters/PermissionRegistry.h"
#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>

namespace modules::audit {

namespace {

bool validAction(const std::string& action) {
    return action.size() <= 64
        && std::all_of(
            action.begin(),
            action.end(),
            [](unsigned char character) {
                return std::islower(character)
                    || std::isdigit(character)
                    || character == '.'
                    || character == '_'
                    || character == '-';
            });
}

bool validIdentifier(
    const std::string& value,
    size_t maxLength,
    size_t minLength = 0) {
    if (value.empty()) return true;
    return value.size() >= minLength
        && value.size() <= maxLength
        && std::all_of(
            value.begin(),
            value.end(),
            [](unsigned char character) {
                return std::isalnum(character)
                    || character == '.'
                    || character == '_'
                    || character == '-'
                    || character == ':'
                    || character == '@';
            });
}

} // namespace

AuditController::AuditController() {
    filters::PermissionRegistry::instance().bind(
        "GET", "/api/audit-logs", "audit:view");
}

drogon::AsyncTask AuditController::list(
    drogon::HttpRequestPtr request,
    std::function<void(const drogon::HttpResponsePtr&)> callback) {
    dto::AuditLogQuery query;
    try {
        const auto parseInteger = [](const std::string& value) -> int64_t {
            size_t parsed = 0;
            const auto result = std::stoll(value, &parsed);
            if (parsed != value.size())
                throw std::invalid_argument("trailing characters");
            return result;
        };
        const auto& parameters = request->getParameters();
        if (const auto it = parameters.find("page");
            it != parameters.end()) {
            const auto value = parseInteger(it->second);
            if (value < std::numeric_limits<int>::min()
                || value > std::numeric_limits<int>::max()) {
                throw std::out_of_range("page");
            }
            query.page = static_cast<int>(value);
        }
        if (const auto it = parameters.find("page_size");
            it != parameters.end()) {
            const auto value = parseInteger(it->second);
            if (value < std::numeric_limits<int>::min()
                || value > std::numeric_limits<int>::max()) {
                throw std::out_of_range("page_size");
            }
            query.pageSize = static_cast<int>(value);
        }
        if (const auto it = parameters.find("action");
            it != parameters.end()) {
            query.action = it->second;
        }
        if (const auto it = parameters.find("actor_user_id");
            it != parameters.end()) {
            query.actorUserId = parseInteger(it->second);
        }
        if (const auto it = parameters.find("target_type");
            it != parameters.end()) {
            query.targetType = it->second;
        }
        if (const auto it = parameters.find("target_id");
            it != parameters.end()) {
            query.targetId = it->second;
        }
        if (const auto it = parameters.find("request_id");
            it != parameters.end()) {
            query.requestId = it->second;
        }
    } catch (const std::exception&) {
        callback(core::Result::fail(
            4002,
            "pagination and actor_user_id must be integers",
            drogon::k400BadRequest));
        co_return;
    }

    if (query.page < 1 || query.page > 1000000
        || query.pageSize < 1 || query.pageSize > 200
        || query.actorUserId < 0
        || !validAction(query.action)
        || !validIdentifier(query.targetType, 32)
        || !validIdentifier(query.targetId, 128)
        || !validIdentifier(query.requestId, 128, 8)) {
        callback(core::Result::fail(
            4002,
            "invalid audit log query",
            drogon::k400BadRequest));
        co_return;
    }

    try {
        const auto result =
            co_await AuditService::instance().repo().listPaged(
                query);
        Json::Value items(Json::arrayValue);
        for (const auto& item : result.items)
            items.append(item.toJson());
        Json::Value data;
        data["items"] = std::move(items);
        data["total"] = static_cast<Json::Int64>(result.total);
        data["page"] = query.page;
        data["page_size"] = query.pageSize;
        callback(core::Result::ok(std::move(data)));
    } catch (const std::exception& error) {
        callback(core::internalError(
            request, "list audit logs", error, 5003));
    }
}

} // namespace modules::audit
