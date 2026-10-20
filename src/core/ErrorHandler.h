#pragma once

#include "Logger.h"
#include "RequestContext.h"
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
    APP_LOG_ERROR << "request failed operation=" << operation
                  << " request_id=" << requestId(req)
                  << " path_template=" << pathTemplate(req)
                  << " error=" << error.what();
    return Result::fail(
        code, "internal server error", drogon::k500InternalServerError);
}

} // namespace core
