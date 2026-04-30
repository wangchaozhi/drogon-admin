#include "PermissionRegistry.h"

namespace filters {

PermissionRegistry& PermissionRegistry::instance() {
    static PermissionRegistry inst;
    return inst;
}

void PermissionRegistry::bind(const std::string& method,
                              const std::string& pathPattern,
                              const std::string& permCode) {
    std::lock_guard<std::mutex> lk(mu_);
    Entry e;
    e.method = method;
    e.re     = std::regex("^" + pathPattern + "/?$");
    e.code   = permCode;
    entries_.push_back(std::move(e));
}

std::optional<std::string> PermissionRegistry::lookup(const std::string& method,
                                                     const std::string& path) const {
    std::lock_guard<std::mutex> lk(mu_);
    for (const auto& e : entries_) {
        if (e.method != method) continue;
        if (std::regex_match(path, e.re)) return e.code;
    }
    return std::nullopt;
}

} // namespace filters
