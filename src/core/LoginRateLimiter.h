#pragma once

#include <chrono>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace core {

class LoginRateLimiter {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    LoginRateLimiter() = default;

    struct Decision {
        bool allowed{true};
        int retryAfterSeconds{0};
        bool globalLimit{false};
    };

    static LoginRateLimiter& instance();

    void configure(int globalLimit,
                   int identifierLimit,
                   int windowSeconds,
                   size_t maxIdentifiers);
    Decision check(std::string_view identifier);

    // Deterministic clock entry point for unit tests.
    Decision checkAt(std::string_view identifier, TimePoint now);

private:
    struct IdentifierWindow {
        std::deque<TimePoint> attempts;
        TimePoint lastSeen;
    };

    void prune(std::deque<TimePoint>& attempts, TimePoint now) const;
    int retryAfter(const std::deque<TimePoint>& attempts,
                   TimePoint now) const;

    std::mutex mutex_;
    int globalLimit_{120};
    int identifierLimit_{10};
    std::chrono::seconds window_{60};
    size_t maxIdentifiers_{10000};
    std::deque<TimePoint> globalAttempts_;
    std::unordered_map<std::string, IdentifierWindow> identifiers_;
};

} // namespace core
