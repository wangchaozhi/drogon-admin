#include "Lifecycle.h"

namespace core {

Lifecycle& Lifecycle::instance() {
    static Lifecycle lifecycle;
    return lifecycle;
}

void Lifecycle::configureGracePeriod(int seconds) {
    gracePeriodSeconds_.store(seconds, std::memory_order_release);
}

int Lifecycle::gracePeriodSeconds() const noexcept {
    return gracePeriodSeconds_.load(std::memory_order_acquire);
}

void Lifecycle::markReady() noexcept {
    auto expected = ServiceState::starting;
    state_.compare_exchange_strong(
        expected,
        ServiceState::ready,
        std::memory_order_acq_rel);
}

bool Lifecycle::beginDraining() noexcept {
    auto current = state_.load(std::memory_order_acquire);
    while (current == ServiceState::starting
           || current == ServiceState::ready) {
        if (state_.compare_exchange_weak(
                current,
                ServiceState::draining,
                std::memory_order_acq_rel)) {
            return true;
        }
    }
    return false;
}

void Lifecycle::markStopped() noexcept {
    state_.store(ServiceState::stopped, std::memory_order_release);
    drainCondition_.notify_all();
}

bool Lifecycle::isReady() const noexcept {
    return state() == ServiceState::ready;
}

bool Lifecycle::isDraining() const noexcept {
    return state() == ServiceState::draining;
}

ServiceState Lifecycle::state() const noexcept {
    return state_.load(std::memory_order_acquire);
}

std::string_view Lifecycle::stateName() const noexcept {
    switch (state()) {
    case ServiceState::starting:
        return "starting";
    case ServiceState::ready:
        return "ready";
    case ServiceState::draining:
        return "draining";
    case ServiceState::stopped:
        return "stopped";
    }
    return "unknown";
}

void Lifecycle::requestStarted() noexcept {
    activeRequests_.fetch_add(1, std::memory_order_acq_rel);
}

void Lifecycle::requestCompleted() noexcept {
    auto current = activeRequests_.load(std::memory_order_acquire);
    while (current > 0) {
        if (activeRequests_.compare_exchange_weak(
                current,
                current - 1,
                std::memory_order_acq_rel)) {
            if (current == 1) drainCondition_.notify_all();
            return;
        }
    }
}

int64_t Lifecycle::activeRequests() const noexcept {
    return activeRequests_.load(std::memory_order_acquire);
}

bool Lifecycle::waitUntilDrained(std::chrono::milliseconds timeout) {
    std::unique_lock lock(drainMutex_);
    drainCondition_.wait_for(
        lock,
        timeout,
        [this] {
            return activeRequests() == 0
                || state() == ServiceState::stopped;
        });
    return activeRequests() == 0;
}

} // namespace core
