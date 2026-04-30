#pragma once
//
// 统一 HTTP 响应封装。所有 Controller 都应使用该结构体返回数据，
// 保证前端拿到的 JSON 结构一致：{ code, message, data }
//
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <string>
#include <utility>

namespace core {

class Result {
public:
    static drogon::HttpResponsePtr ok(Json::Value data = Json::nullValue,
                                      const std::string& message = "ok") {
        return build(0, message, std::move(data), drogon::k200OK);
    }

    static drogon::HttpResponsePtr fail(int code,
                                        const std::string& message,
                                        drogon::HttpStatusCode http = drogon::k400BadRequest) {
        return build(code, message, Json::nullValue, http);
    }

    static drogon::HttpResponsePtr unauthorized(const std::string& message = "unauthorized") {
        return build(401, message, Json::nullValue, drogon::k401Unauthorized);
    }

    static drogon::HttpResponsePtr notFound(const std::string& message = "not found") {
        return build(404, message, Json::nullValue, drogon::k404NotFound);
    }

private:
    static drogon::HttpResponsePtr build(int code,
                                         const std::string& message,
                                         Json::Value data,
                                         drogon::HttpStatusCode http) {
        Json::Value body;
        body["code"]    = code;
        body["message"] = message;
        body["data"]    = std::move(data);
        auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
        resp->setStatusCode(http);
        return resp;
    }
};

} // namespace core
