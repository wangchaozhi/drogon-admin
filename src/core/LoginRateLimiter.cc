#include "LoginRateLimiter.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace core {

LoginRateLimiter& LoginRateLimiter::instance() {
    static LoginRateLimiter limiter;
    return limiter;
}

void LoginRateLimiter::configure(int globalLimit,
                                 int identifierLimit,
                                 int windowSeconds,
                                 size_t maxIdentifiers) {
    if (globalLimit < 1 || identifierLimit < 1 || windowSeconds < 1
        || maxIdentifiers < 1) {
        throw std::invalid_argument("login rate limit values must be positive");
    }

    std::lock_guard lock(mutex_);
    globalLimit_ = globalLimit;
    identifierLimit_ = identifierLimit;
    window_ = std::chrono::seconds(windowSeconds);
    maxIdentifiers_ = maxIdentifiers;
    globalAttempts_.clear();
    identifiers_.clear();
}

LoginRateLimiter::Decision LoginRateLimiter::check(
    std::string_view identifier) {
    return checkAt(identifier, Clock::now());
}

LoginRateLimiter::Decision LoginRateLimiter::checkAt(
    std::string_view identifier,
    TimePoint now) {
    std::lock_guard lock(mutex_);

    prune(globalAttempts_, now);
    if (globalAttempts_.size() >= static_cast<size_t>(globalLimit_)) {
        return {false, retryAfter(globalAttempts_, now), true};
    }

    const std::string key(identifier);
    auto found = identifiers_.find(key);
    if (found == identifiers_.end()) {
        if (identifiers_.size() >= maxIdentifiers_) {
            // The global window still bounds identifier-churn attacks. Evicting
            // one entry keeps memory usage deterministic without an O(n) scan.
            identifiers_.erase(identifiers_.begin());
        }
        found = identifiers_
                    .emplace(key, IdentifierWindow{{}, now})
                    .first;
    }

    auto& identifierWindow = found->second;
    prune(identifierWindow.attempts, now);
    identifierWindow.lastSeen = now;
    if (identifierWindow.attempts.size()
        >= static_cast<size_t>(identifierLimit_)) {
        return {
            false,
            retryAfter(identifierWindow.attempts, now),
            false,
        };
    }

    globalAttempts_.push_back(now);
    identifierWindow.attempts.push_back(now);
    return {};
}

void LoginRateLimiter::prune(std::deque<TimePoint>& attempts,
                             TimePoint now) const {
    const auto cutoff = now - window_;
    while (!attempts.empty() && attempts.front() <= cutoff)
        attempts.pop_front();
}

int LoginRateLimiter::retryAfter(
    const std::deque<TimePoint>& attempts,
    TimePoint now) const {
    if (attempts.empty()) return 1;
    const auto remaining = attempts.front() + window_ - now;
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(remaining)
            .count();
    return std::max(
        1,
        static_cast<int>(std::ceil(
            static_cast<double>(milliseconds) / 1000.0)));
}

} // namespace core
