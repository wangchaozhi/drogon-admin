#include "Metrics.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#ifndef DROGON_ADMIN_VERSION
#define DROGON_ADMIN_VERSION "unknown"
#endif

namespace core {

Metrics& Metrics::instance() {
    static Metrics metrics;
    return metrics;
}

void Metrics::requestStarted() noexcept {
    activeRequests_.fetch_add(1, std::memory_order_relaxed);
}

void Metrics::requestCompleted(
    int statusCode,
    int64_t durationMicros) noexcept {
    activeRequests_.fetch_sub(1, std::memory_order_relaxed);
    completedRequests_.fetch_add(1, std::memory_order_relaxed);

    const auto safeDuration =
        static_cast<uint64_t>(std::max<int64_t>(0, durationMicros));
    durationMicros_.fetch_add(safeDuration, std::memory_order_relaxed);
    for (size_t index = 0; index < kDurationBucketsMicros.size(); ++index) {
        if (safeDuration
            <= static_cast<uint64_t>(kDurationBucketsMicros[index])) {
            durationBuckets_[index].fetch_add(
                1, std::memory_order_relaxed);
        }
    }

    const int statusClass = statusCode / 100;
    if (statusClass >= 1 && statusClass <= 5) {
        statusClasses_[static_cast<size_t>(statusClass - 1)].fetch_add(
            1, std::memory_order_relaxed);
    }
}

std::string Metrics::renderPrometheus() const {
    std::ostringstream output;
    output << std::fixed << std::setprecision(6);

    output
        << "# HELP drogon_admin_build_info Build and version information.\n"
        << "# TYPE drogon_admin_build_info gauge\n"
        << "drogon_admin_build_info{version=\""
        << DROGON_ADMIN_VERSION << "\"} 1\n"
        << "# HELP drogon_admin_uptime_seconds Process uptime in seconds.\n"
        << "# TYPE drogon_admin_uptime_seconds gauge\n"
        << "drogon_admin_uptime_seconds "
        << std::chrono::duration<double>(
               std::chrono::steady_clock::now() - startedAt_)
               .count()
        << '\n'
        << "# HELP drogon_admin_http_requests_active "
           "Requests currently being processed.\n"
        << "# TYPE drogon_admin_http_requests_active gauge\n"
        << "drogon_admin_http_requests_active "
        << activeRequests_.load(std::memory_order_relaxed) << '\n'
        << "# HELP drogon_admin_http_requests_total "
           "Completed HTTP requests by status class.\n"
        << "# TYPE drogon_admin_http_requests_total counter\n";

    for (size_t index = 0; index < statusClasses_.size(); ++index) {
        output << "drogon_admin_http_requests_total{status_class=\""
               << index + 1 << "xx\"} "
               << statusClasses_[index].load(std::memory_order_relaxed)
               << '\n';
    }

    output
        << "# HELP drogon_admin_http_request_duration_seconds "
           "HTTP request duration in seconds.\n"
        << "# TYPE drogon_admin_http_request_duration_seconds histogram\n";
    for (size_t index = 0; index < kDurationBucketsMicros.size(); ++index) {
        output
            << "drogon_admin_http_request_duration_seconds_bucket{le=\""
            << static_cast<double>(kDurationBucketsMicros[index])
                   / 1'000'000.0
            << "\"} "
            << durationBuckets_[index].load(std::memory_order_relaxed)
            << '\n';
    }
    const auto completed =
        completedRequests_.load(std::memory_order_relaxed);
    output
        << "drogon_admin_http_request_duration_seconds_bucket{le=\"+Inf\"} "
        << completed << '\n'
        << "drogon_admin_http_request_duration_seconds_sum "
        << static_cast<double>(
               durationMicros_.load(std::memory_order_relaxed))
               / 1'000'000.0
        << '\n'
        << "drogon_admin_http_request_duration_seconds_count "
        << completed << '\n';
    return output.str();
}

} // namespace core
