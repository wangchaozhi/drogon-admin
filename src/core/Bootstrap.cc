#include "Bootstrap.h"
#include "Logger.h"
#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <trantor/utils/AsyncFileLogger.h>
#include <trantor/utils/Logger.h>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace core {

namespace {

// 读取整份 SQL 文件并按分号切分为多条语句（忽略空语句和纯注释行）
std::vector<std::string> loadSqlStatements(const std::string& path) {
    std::vector<std::string> stmts;
    std::ifstream ifs(path);
    if (!ifs) {
        APP_LOG_WARN << "schema file not found: " << path;
        return stmts;
    }
    std::stringstream ss;
    ss << ifs.rdbuf();
    std::string all = ss.str();

    std::string cur;
    for (char c : all) {
        if (c == ';') {
            // 过滤整段注释 / 空白
            std::string trimmed;
            trimmed.reserve(cur.size());
            bool inLineComment = false;
            for (size_t i = 0; i < cur.size(); ++i) {
                if (inLineComment) {
                    if (cur[i] == '\n') inLineComment = false;
                    continue;
                }
                if (i + 1 < cur.size() && cur[i] == '-' && cur[i + 1] == '-') {
                    inLineComment = true;
                    ++i;
                    continue;
                }
                trimmed.push_back(cur[i]);
            }
            // 判断非空白
            if (trimmed.find_first_not_of(" \t\r\n") != std::string::npos) {
                stmts.push_back(trimmed);
            }
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    return stmts;
}

void ensureDbDirAndSchema() {
    // 确保 data / logs / uploads 目录存在（均为配置里引用的相对路径）
    std::error_code ec;
    std::filesystem::create_directories("data", ec);
    std::filesystem::create_directories("logs", ec);
    std::filesystem::create_directories("uploads", ec);

    // 待 Drogon 启动后再执行 schema，使用 beginningAdvice 钩子
    drogon::app().registerBeginningAdvice([]() {
        auto db = drogon::app().getDbClient();
        if (!db) {
            APP_LOG_ERROR << "default DbClient is null, skip schema init";
            return;
        }
        auto stmts = loadSqlStatements("sql/schema.sql");
        for (const auto& s : stmts) {
            try {
                db->execSqlSync(s);
            } catch (const std::exception& e) {
                // 幂等迁移：列/表/索引已存在属于预期，无需 ERROR 噪音
                std::string msg = e.what();
                auto isBenign = msg.find("duplicate column name") != std::string::npos
                             || msg.find("already exists") != std::string::npos;
                if (isBenign) {
                    APP_LOG_DEBUG << "schema stmt skipped (already applied): " << msg;
                } else {
                    APP_LOG_ERROR << "schema stmt failed: " << msg
                                  << " | sql=" << s;
                }
            }
        }
        APP_LOG_INFO << "schema.sql applied, statements=" << stmts.size();
    });
}

} // namespace

// 进程级别的异步文件日志器，生命周期与进程一致
static trantor::AsyncFileLogger& getAsyncFileLogger() {
    static trantor::AsyncFileLogger logger;
    return logger;
}

// 异步控制台输出管道：
//  - 业务线程仅入队（O(1) 拷贝 + 全局锁），不直接 fwrite(stdout)；
//  - 后台线程批量刷 stdout，Windows 下即使用户开启了 QuickEdit
//    "选定"阻塞了控制台写入，阻塞的也只是消费线程，不会拖住 HTTP 线程。
class AsyncConsoleSink {
public:
    static AsyncConsoleSink& instance() {
        static AsyncConsoleSink s;
        return s;
    }

    void push(const char* msg, uint64_t len) {
        {
            std::lock_guard<std::mutex> lk(mu_);
            // 背压保护：队列堆积过多时丢弃最旧日志，避免内存无限膨胀
            if (queued_bytes_ > kMaxQueueBytes) {
                while (!queue_.empty() && queued_bytes_ > kMaxQueueBytes / 2) {
                    queued_bytes_ -= queue_.front().size();
                    queue_.pop_front();
                }
            }
            queue_.emplace_back(msg, msg + len);
            queued_bytes_ += static_cast<size_t>(len);
        }
        cv_.notify_one();
    }

    void flush() {
        cv_.notify_one();
    }

private:
    AsyncConsoleSink() {
        worker_ = std::thread([this] { loop(); });
        worker_.detach();   // 随进程退出，不阻塞 shutdown
    }

    void loop() {
        std::deque<std::string> local;
        for (;;) {
            {
                std::unique_lock<std::mutex> lk(mu_);
                cv_.wait(lk, [this] { return !queue_.empty(); });
                local.swap(queue_);
                queued_bytes_ = 0;
            }
            for (auto& s : local) {
                std::fwrite(s.data(), 1, s.size(), stdout);
            }
            local.clear();
            std::fflush(stdout);
        }
    }

    std::mutex              mu_;
    std::condition_variable cv_;
    std::deque<std::string> queue_;
    size_t                  queued_bytes_{0};
    std::thread             worker_;
    static constexpr size_t kMaxQueueBytes = 32 * 1024 * 1024; // 32 MB
};

// 让日志同时输出到控制台与文件（双路均为异步）
static void setupDualLogging() {
    auto& fileLogger = getAsyncFileLogger();
    fileLogger.setFileName("drogon-admin", ".log", "./logs");
    fileLogger.setFileSizeLimit(100 * 1000 * 1000);
    fileLogger.startLogging();

    // 预先触发 AsyncConsoleSink 初始化，启动后台线程
    AsyncConsoleSink::instance();

    trantor::Logger::setOutputFunction(
        [](const char* msg, uint64_t len) {
            // 文件日志本身异步；控制台输出经由 AsyncConsoleSink 也成为异步
            getAsyncFileLogger().output(msg, len);
            AsyncConsoleSink::instance().push(msg, len);
        },
        []() {
            getAsyncFileLogger().flush();
            AsyncConsoleSink::instance().flush();
        });
}

void Bootstrap::init(const std::string& configPath) {
    // 先确保配置中引用的相对目录存在，否则 Drogon 会因 "Log path does not exist" 退出
    {
        std::error_code ec;
        std::filesystem::create_directories("data", ec);
        std::filesystem::create_directories("logs", ec);
        std::filesystem::create_directories("uploads", ec);
    }

    auto& app = drogon::app();
    app.loadConfigFile(configPath);

    // config.json 已把 log_path 置空，避免 Drogon 再装一次 file logger；
    // 这里统一装配 "控制台 + 文件" 的双输出。
    setupDualLogging();

    ensureDbDirAndSchema();

    // 启动 banner
    app.registerBeginningAdvice([]() {
        APP_LOG_INFO << "============================================";
        APP_LOG_INFO << "  drogon-admin started (Drogon + SQLite modular)";
        APP_LOG_INFO << "============================================";
    });
}

} // namespace core
