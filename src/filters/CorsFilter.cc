#include "CorsFilter.h"
#include "core/Result.h"
#include "plugins/ConfigPlugin.h"

#include <drogon/drogon.h>
#include <drogon/HttpResponse.h>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <unordered_set>

namespace filters {

namespace {

plugins::ConfigPlugin* config() {
    return drogon::app().getPlugin<plugins::ConfigPlugin>();
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                   });
    return value;
}

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool requestedHeadersAllowed(const std::string& requested) {
    static const std::unordered_set<std::string> allowed{
        "authorization", "content-type", "x-request-id"};
    std::stringstream stream(requested);
    std::string header;
    while (std::getline(stream, header, ',')) {
        header = lower(trim(std::move(header)));
        if (!header.empty() && !allowed.contains(header)) return false;
    }
    return true;
}

bool requestedMethodAllowed(std::string method) {
    std::transform(method.begin(), method.end(), method.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::toupper(c));
                   });
    static const std::unordered_set<std::string> allowed{
        "GET", "POST", "PUT", "DELETE", "PATCH"};
    return allowed.contains(method);
}

} // namespace

bool CorsFilter::isPreflight(const drogon::HttpRequestPtr& req) {
    return req->method() == drogon::Options
        && !req->getHeader("Origin").empty()
        && !req->getHeader("Access-Control-Request-Method").empty();
}

drogon::HttpResponsePtr CorsFilter::handlePreflight(
    const drogon::HttpRequestPtr& req) {
    auto* cfg = config();
    if (!cfg) {
        auto resp = core::Result::fail(
            5005, "configuration unavailable",
            drogon::k500InternalServerError);
        applyResponseHeaders(req, resp);
        return resp;
    }

    const auto& origin = req->getHeader("Origin");
    if (!cfg->isCorsOriginAllowed(origin)
        || !requestedMethodAllowed(
            req->getHeader("Access-Control-Request-Method"))
        || !requestedHeadersAllowed(
            req->getHeader("Access-Control-Request-Headers"))) {
        auto resp = core::Result::fail(
            4032, "cross-origin request is not allowed",
            drogon::k403Forbidden);
        applyResponseHeaders(req, resp);
        return resp;
    }

    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k204NoContent);
    applyResponseHeaders(req, resp);
    return resp;
}

void CorsFilter::applyResponseHeaders(
    const drogon::HttpRequestPtr& req,
    const drogon::HttpResponsePtr& resp) {
    auto* cfg = config();
    if (!cfg) return;

    if (cfg->hstsEnabled()) {
        resp->addHeader(
            "Strict-Transport-Security",
            "max-age=31536000; includeSubDomains");
    }

    if (req->getPath().rfind("/api/", 0) != 0
        && req->getPath() != "/api") {
        return;
    }

    resp->addHeader("X-Content-Type-Options", "nosniff");
    resp->addHeader("X-Frame-Options", "DENY");
    resp->addHeader("Referrer-Policy", "no-referrer");
    resp->addHeader(
        "Content-Security-Policy",
        "default-src 'none'; frame-ancestors 'none'");
    if (req->getPath().rfind("/api/auth/", 0) == 0)
        resp->addHeader("Cache-Control", "no-store");

    const auto& origin = req->getHeader("Origin");
    if (origin.empty() || !cfg->isCorsOriginAllowed(origin)) return;

    resp->addHeader("Access-Control-Allow-Origin", origin);
    resp->addHeader(
        "Access-Control-Expose-Headers",
        "Retry-After,X-Request-ID");
    if (cfg->corsAllowCredentials())
        resp->addHeader("Access-Control-Allow-Credentials", "true");

    if (isPreflight(req)) {
        resp->addHeader(
            "Access-Control-Allow-Methods",
            "GET,POST,PUT,DELETE,PATCH,OPTIONS");
        resp->addHeader(
            "Access-Control-Allow-Headers",
            "Authorization,Content-Type,X-Request-ID");
        resp->addHeader(
            "Access-Control-Max-Age",
            std::to_string(cfg->corsMaxAgeSeconds()));
        resp->addHeader(
            "Vary",
            "Origin, Access-Control-Request-Method, "
            "Access-Control-Request-Headers");
    } else {
        resp->addHeader("Vary", "Origin");
    }
}

void CorsFilter::doFilter(const drogon::HttpRequestPtr& req,
                          drogon::FilterCallback&& fcb,
                          drogon::FilterChainCallback&& fccb) {
    if (isPreflight(req)) {
        fcb(handlePreflight(req));
        return;
    }
    fccb();
}

} // namespace filters
