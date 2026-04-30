#pragma once
//
// 时间工具：给业务层提供统一的时间戳/格式化函数，避免到处 time()、chrono 混用。
//
#include <chrono>
#include <ctime>
#include <string>

namespace common {

class TimeUtil {
public:
    // 秒级时间戳
    static int64_t nowSec() {
        return std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

    // 毫秒级时间戳
    static int64_t nowMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

    // "YYYY-MM-DD HH:MM:SS"（本地时区）
    static std::string formatLocal(int64_t epochSec) {
        std::time_t t = static_cast<std::time_t>(epochSec);
        std::tm tmv{};
#ifdef _WIN32
        localtime_s(&tmv, &t);
#else
        localtime_r(&t, &tmv);
#endif
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv);
        return std::string(buf);
    }
};

} // namespace common
