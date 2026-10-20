#pragma once

#include <drogon/HttpRequest.h>
#include <string>

namespace core {

inline std::string requestId(const drogon::HttpRequestPtr& request) {
    if (!request) return "unknown";
    const auto attributes = request->attributes();
    return attributes && attributes->find("requestId")
        ? attributes->get<std::string>("requestId")
        : "unknown";
}

inline std::string pathTemplate(const drogon::HttpRequestPtr& request) {
    if (!request) return "<unmatched>";
    const auto matched = request->getMatchedPathPattern();
    return matched.empty() ? "<unmatched>" : std::string(matched);
}

} // namespace core
