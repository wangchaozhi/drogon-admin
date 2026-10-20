#pragma once

#include "Logger.h"
#include "Result.h"
#include <drogon/HttpRequest.h>
#include <exception>
#include <string>
#include <string_view>

namespace core {

inline drogon::HttpResponsePtr internalError(
    const drogon::HttpRequestPtr& req,
    std::string_view operation,
    const std::exception& error,
    int code = 5000) {
    std::string requestId = "unknown";
    if (req) {
        const auto attrs = req->attributes();
        if (attrs && attrs->find("requestId"))
            requestId = attrs->get<std::string>("requestId");
    }

    APP_LOG_ERROR << "request failed operation=" << operation
                  << " request_id=" << requestId
                  << " error=" << error.what();
    return Result::fail(
        code, "internal server error", drogon::k500InternalServerError);
}

} // namespace core
