#include "core/LoginRateLimiter.h"

#include <cassert>
#include <chrono>

using namespace std::chrono_literals;

int main() {
    core::LoginRateLimiter limiter;
    const core::LoginRateLimiter::TimePoint start{};

    limiter.configure(5, 3, 60, 2);
    assert(limiter.checkAt("alice", start).allowed);
    assert(limiter.checkAt("alice", start + 1s).allowed);
    assert(limiter.checkAt("alice", start + 2s).allowed);

    const auto identifierBlocked =
        limiter.checkAt("alice", start + 3s);
    assert(!identifierBlocked.allowed);
    assert(!identifierBlocked.globalLimit);
    assert(identifierBlocked.retryAfterSeconds == 57);

    assert(limiter.checkAt("bob", start + 3s).allowed);
    assert(limiter.checkAt("bob", start + 4s).allowed);
    const auto globalBlocked = limiter.checkAt("carol", start + 5s);
    assert(!globalBlocked.allowed);
    assert(globalBlocked.globalLimit);
    assert(globalBlocked.retryAfterSeconds == 55);

    // Both windows expire exactly at the configured boundary.
    assert(limiter.checkAt("alice", start + 60s).allowed);

    // Reconfiguration resets state and enforces a bounded identifier map.
    limiter.configure(10, 3, 10, 1);
    assert(limiter.checkAt("first", start).allowed);
    assert(limiter.checkAt("second", start).allowed);
    assert(limiter.checkAt("second", start + 1s).allowed);
    assert(limiter.checkAt("second", start + 2s).allowed);
    const auto boundedMapBlocked =
        limiter.checkAt("second", start + 3s);
    assert(!boundedMapBlocked.allowed);
    assert(!boundedMapBlocked.globalLimit);
    assert(boundedMapBlocked.retryAfterSeconds == 7);

    return 0;
}
