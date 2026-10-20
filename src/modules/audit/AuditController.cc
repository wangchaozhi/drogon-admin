#include "AuditController.h"
#include "AuditService.h"
#include "core/ErrorHandler.h"
#include "core/Result.h"
#include "filters/PermissionRegistry.h"
#include <algorithm>
#include <cctype>
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

} // namespace

AuditController::AuditController() {
    filters::PermissionRegistry::instance().bind(
        "GET", "/api/audit-logs", "audit:view");
}

drogon::AsyncTask AuditController::list(
    drogon::HttpRequestPtr request,
    std::function<void(const drogon::HttpResponsePtr&)> callback) {
    int page = 1;
    int pageSize = 20;
    std::string action;
    try {
        const auto parseInteger = [](const std::string& value) {
            size_t parsed = 0;
            const int result = std::stoi(value, &parsed);
            if (parsed != value.size())
                throw std::invalid_argument("trailing characters");
            return result;
        };
        const auto& parameters = request->getParameters();
        if (const auto it = parameters.find("page");
            it != parameters.end()) {
            page = parseInteger(it->second);
        }
        if (const auto it = parameters.find("page_size");
            it != parameters.end()) {
            pageSize = parseInteger(it->second);
        }
        if (const auto it = parameters.find("action");
            it != parameters.end()) {
            action = it->second;
        }
    } catch (const std::exception&) {
        callback(core::Result::fail(
            4002,
            "page and page_size must be integers",
            drogon::k400BadRequest));
        co_return;
    }

    if (page < 1 || page > 1000000
        || pageSize < 1 || pageSize > 200
        || !validAction(action)) {
        callback(core::Result::fail(
            4002,
            "invalid audit log query",
            drogon::k400BadRequest));
        co_return;
    }

    try {
        const auto result =
            co_await AuditService::instance().repo().listPaged(
                page, pageSize, std::move(action));
        Json::Value items(Json::arrayValue);
        for (const auto& item : result.items)
            items.append(item.toJson());
        Json::Value data;
        data["items"] = std::move(items);
        data["total"] = static_cast<Json::Int64>(result.total);
        data["page"] = page;
        data["page_size"] = pageSize;
        callback(core::Result::ok(std::move(data)));
    } catch (const std::exception& error) {
        callback(core::internalError(
            request, "list audit logs", error, 5003));
    }
}

} // namespace modules::audit
