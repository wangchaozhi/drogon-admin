#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string_view>

namespace core {

enum class ServiceState : uint8_t {
    starting,
    ready,
    draining,
    stopped,
};

class Lifecycle {
public:
    Lifecycle() = default;

    static Lifecycle& instance();

    void configureGracePeriod(int seconds);
    int gracePeriodSeconds() const noexcept;

    void markReady() noexcept;
    bool beginDraining() noexcept;
    void markStopped() noexcept;

    bool isReady() const noexcept;
    bool isDraining() const noexcept;
    ServiceState state() const noexcept;
    std::string_view stateName() const noexcept;

    void requestStarted() noexcept;
    void requestCompleted() noexcept;
    int64_t activeRequests() const noexcept;
    bool waitUntilDrained(std::chrono::milliseconds timeout);

private:
    std::atomic<ServiceState> state_{ServiceState::starting};
    std::atomic<int> gracePeriodSeconds_{10};
    std::atomic<int64_t> activeRequests_{0};
    mutable std::mutex drainMutex_;
    std::condition_variable drainCondition_;
};

} // namespace core
