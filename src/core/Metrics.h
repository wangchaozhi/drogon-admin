#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

namespace core {

class Metrics {
public:
    static Metrics& instance();

    void requestStarted() noexcept;
    void requestCompleted(int statusCode, int64_t durationMicros) noexcept;
    void loginRateLimited(bool globalLimit) noexcept;
    std::string renderPrometheus() const;

private:
    Metrics() = default;

    static constexpr std::array<int64_t, 11> kDurationBucketsMicros{
        5'000,
        10'000,
        25'000,
        50'000,
        100'000,
        250'000,
        500'000,
        1'000'000,
        2'500'000,
        5'000'000,
        10'000'000,
    };

    std::chrono::steady_clock::time_point startedAt_{
        std::chrono::steady_clock::now()};
    std::atomic<int64_t> activeRequests_{0};
    std::atomic<uint64_t> completedRequests_{0};
    std::atomic<uint64_t> durationMicros_{0};
    std::array<std::atomic<uint64_t>, 5> statusClasses_{};
    std::array<std::atomic<uint64_t>, kDurationBucketsMicros.size()>
        durationBuckets_{};
    std::array<std::atomic<uint64_t>, 2> loginRateLimited_{};
};

} // namespace core
