#include "core/Lifecycle.h"

#include <cassert>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

int main() {
    core::Lifecycle lifecycle;

    assert(lifecycle.state() == core::ServiceState::starting);
    assert(!lifecycle.isReady());
    lifecycle.configureGracePeriod(17);
    assert(lifecycle.gracePeriodSeconds() == 17);

    lifecycle.markReady();
    assert(lifecycle.isReady());
    assert(lifecycle.stateName() == "ready");

    lifecycle.requestStarted();
    lifecycle.requestStarted();
    assert(lifecycle.activeRequests() == 2);
    assert(lifecycle.beginDraining());
    assert(!lifecycle.beginDraining());
    assert(lifecycle.isDraining());
    assert(lifecycle.stateName() == "draining");

    std::thread completer([&lifecycle] {
        std::this_thread::sleep_for(20ms);
        lifecycle.requestCompleted();
        lifecycle.requestCompleted();
    });
    assert(lifecycle.waitUntilDrained(1s));
    completer.join();
    assert(lifecycle.activeRequests() == 0);

    lifecycle.requestCompleted();
    assert(lifecycle.activeRequests() == 0);
    lifecycle.markReady();
    assert(lifecycle.isDraining());

    lifecycle.markStopped();
    assert(lifecycle.state() == core::ServiceState::stopped);
    assert(lifecycle.stateName() == "stopped");
    return 0;
}
