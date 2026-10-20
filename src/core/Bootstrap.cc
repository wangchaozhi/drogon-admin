#include "Bootstrap.h"
#include "Lifecycle.h"
#include "Logger.h"
#include "Metrics.h"
#include "Result.h"
#include "common/CryptoUtil.h"
#include "filters/CorsFilter.h"
#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <trantor/utils/AsyncFileLogger.h>
#include <trantor/utils/Logger.h>
#include <algorithm>
#include <cctype>
#include <csignal>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace core {

namespace {

int64_t steadyNowMicros() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool validRequestId(const std::string& value) {
    return value.size() >= 8
        && value.size() <= 128
        && std::all_of(
            value.begin(), value.end(),
            [](unsigned char c) {
                return std::isalnum(c)
                    || c == '-'
                    || c == '_'
                    || c == '.';
            });
}

void prepareRequest(const drogon::HttpRequestPtr& req) {
    auto attrs = req->attributes();
    if (attrs->find("requestId")) return;
    auto requestId = req->getHeader("X-Request-ID");
    if (!validRequestId(requestId))
        requestId = drogon::utils::genRandomString(24);
    attrs->insert("requestId", std::move(requestId));
    attrs->insert("requestStartedAtMicros", steadyNowMicros());
    Metrics::instance().requestStarted();
    Lifecycle::instance().requestStarted();
}

void completeRequest(const drogon::HttpRequestPtr& req,
                     const drogon::HttpResponsePtr& resp) {
    auto attrs = req->attributes();
    if (attrs->find("requestCompleted")) return;
    if (!attrs->find("requestId")) prepareRequest(req);
    attrs->insert("requestCompleted", true);

    const auto requestId = attrs->get<std::string>("requestId");
    const auto startedAt =
        attrs->get<int64_t>("requestStartedAtMicros");
    const auto elapsedMicros =
        std::max<int64_t>(0, steadyNowMicros() - startedAt);
    resp->addHeader("X-Request-ID", requestId);
    Metrics::instance().requestCompleted(
        static_cast<int>(resp->statusCode()), elapsedMicros);
    Lifecycle::instance().requestCompleted();

    std::ostringstream details;
    details << "request completed request_id=" << requestId
            << " method=" << req->methodString()
            << " path=" << req->getPath()
            << " status=" << static_cast<int>(resp->statusCode())
            << " duration_us=" << elapsedMicros;
    if (attrs->find("userId"))
        details << " user_id=" << attrs->get<int64_t>("userId");
    if (req->getPath() == "/api/health"
        || req->getPath() == "/api/ready"
        || req->getPath() == "/api/metrics") {
        APP_LOG_DEBUG << details.str();
    } else {
        APP_LOG_INFO << details.str();
    }
}

// 读取 SQL 文件并按分号切分为多条语句（忽略空语句和纯注释行）。
std::vector<std::string> loadSqlStatements(
    const std::filesystem::path& path) {
    std::vector<std::string> stmts;
    std::ifstream ifs(path);
    if (!ifs)
        throw std::runtime_error(
            "SQL file not found: " + path.string());
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

std::string fileSha256(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error(
            "SQL file not found: " + path.string());
    std::ostringstream contents;
    contents << input.rdbuf();
    if (input.bad())
        throw std::runtime_error(
            "SQL file read failed: " + path.string());
    const auto raw = contents.str();
    std::string normalized;
    normalized.reserve(raw.size());
    for (size_t index = 0; index < raw.size(); ++index) {
        if (raw[index] == '\r') {
            if (index + 1 < raw.size() && raw[index + 1] == '\n')
                ++index;
            normalized.push_back('\n');
        } else {
            normalized.push_back(raw[index]);
        }
    }
    return common::CryptoUtil::sha256Hex(normalized);
}

std::optional<std::pair<std::string, std::string>>
alterAddColumnTarget(const std::string& statement) {
    std::istringstream stream(statement);
    std::string alter;
    std::string tableKeyword;
    std::string table;
    std::string add;
    std::string columnKeyword;
    std::string column;
    stream >> alter >> tableKeyword >> table >> add >> columnKeyword >> column;

    auto lower = [](std::string value) {
        std::transform(
            value.begin(), value.end(), value.begin(),
            [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
        return value;
    };
    if (lower(alter) != "alter"
        || lower(tableKeyword) != "table"
        || lower(add) != "add"
        || lower(columnKeyword) != "column"
        || table.empty()
        || column.empty()) {
        return std::nullopt;
    }
    const auto validIdentifier = [](const std::string& value) {
        return std::all_of(
            value.begin(), value.end(),
            [](unsigned char c) {
                return std::isalnum(c) || c == '_';
            });
    };
    if (!validIdentifier(table) || !validIdentifier(column))
        return std::nullopt;
    return std::pair{std::move(table), std::move(column)};
}

bool columnExists(const drogon::orm::DbClientPtr& db,
                  const std::string& table,
                  const std::string& column) {
    const auto rows = db->execSqlSync(
        "PRAGMA table_info(" + table + ")");
    return std::any_of(
        rows.begin(), rows.end(),
        [&column](const auto& row) {
            return row["name"].template as<std::string>() == column;
        });
}

void configureDatabase(const drogon::orm::DbClientPtr& db) {
    auto journalMode = db->execSqlSync("PRAGMA journal_mode=WAL");
    if (journalMode.empty()
        || journalMode[0][0].as<std::string>() != "wal") {
        throw std::runtime_error("SQLite WAL mode is unavailable");
    }
    db->execSqlSync("PRAGMA synchronous=NORMAL");
    db->execSqlSync("PRAGMA busy_timeout=5000");
    db->execSqlSync("PRAGMA foreign_keys=ON");

    auto foreignKeys = db->execSqlSync("PRAGMA foreign_keys");
    if (foreignKeys.empty() || foreignKeys[0][0].as<int>() != 1)
        throw std::runtime_error("SQLite foreign key enforcement is unavailable");

    APP_LOG_INFO
        << "database configured journalMode=wal synchronous=normal "
           "busyTimeoutMs=5000 foreignKeys=on";
}

void applySchemaAndMigrations(const drogon::orm::DbClientPtr& db) {
    const auto schemaPath = std::filesystem::path("sql") / "schema.sql";
    const auto schemaStatements = loadSqlStatements(schemaPath);
    for (const auto& statement : schemaStatements)
        db->execSqlSync(statement);

    db->execSqlSync(
        "CREATE TABLE IF NOT EXISTS schema_migrations("
        "version TEXT PRIMARY KEY,"
        "checksum TEXT NOT NULL,"
        "applied_at INTEGER NOT NULL"
        ")");
    if (!columnExists(db, "schema_migrations", "checksum")) {
        db->execSqlSync(
            "ALTER TABLE schema_migrations ADD COLUMN checksum TEXT");
    }

    const auto migrationsDir =
        std::filesystem::path("sql") / "migrations";
    if (!std::filesystem::is_directory(migrationsDir))
        throw std::runtime_error(
            "migrations directory not found: " + migrationsDir.string());

    std::vector<std::filesystem::path> migrations;
    for (const auto& entry :
         std::filesystem::directory_iterator(migrationsDir)) {
        if (entry.is_regular_file()
            && entry.path().extension() == ".sql") {
            migrations.push_back(entry.path());
        }
    }
    std::sort(migrations.begin(), migrations.end());

    std::set<std::string> availableVersions;
    for (const auto& migration : migrations) {
        const auto version = migration.stem().string();
        if (!availableVersions.insert(version).second)
            throw std::runtime_error(
                "duplicate migration version: " + version);
    }

    size_t checksumBackfillCount = 0;
    const auto appliedMigrations = db->execSqlSync(
        "SELECT version,COALESCE(checksum,'') AS checksum "
        "FROM schema_migrations ORDER BY version");
    for (const auto& applied : appliedMigrations) {
        const auto version = applied["version"].as<std::string>();
        if (!availableVersions.contains(version))
            throw std::runtime_error(
                "applied migration file is missing: " + version);

        const auto migration = std::find_if(
            migrations.begin(), migrations.end(),
            [&version](const auto& path) {
                return path.stem().string() == version;
            });
        const auto expectedChecksum = fileSha256(*migration);
        const auto storedChecksum =
            applied["checksum"].as<std::string>();
        if (storedChecksum.empty()) {
            db->execSqlSync(
                "UPDATE schema_migrations SET checksum=? "
                "WHERE version=? AND (checksum IS NULL OR checksum='')",
                expectedChecksum,
                version);
            ++checksumBackfillCount;
            APP_LOG_INFO
                << "database migration checksum backfilled: " << version;
        } else if (storedChecksum != expectedChecksum) {
            throw std::runtime_error(
                "migration checksum mismatch: " + version);
        }
    }

    size_t appliedCount = 0;
    for (const auto& migration : migrations) {
        const auto version = migration.stem().string();
        const auto checksum = fileSha256(migration);
        auto applied = db->execSqlSync(
            "SELECT 1 FROM schema_migrations WHERE version=?",
            version);
        if (!applied.empty()) continue;

        db->execSqlSync("BEGIN IMMEDIATE");
        try {
            for (const auto& statement :
                 loadSqlStatements(migration)) {
                const auto alterColumn =
                    alterAddColumnTarget(statement);
                if (alterColumn
                    && columnExists(
                        db, alterColumn->first, alterColumn->second)) {
                    APP_LOG_DEBUG
                        << "migration column already exists version="
                        << version << " table=" << alterColumn->first
                        << " column=" << alterColumn->second;
                    continue;
                }
                db->execSqlSync(statement);
            }
            db->execSqlSync(
                "INSERT INTO schema_migrations(version,checksum,applied_at) "
                "VALUES(?,?,CAST(strftime('%s','now') AS INTEGER))",
                version,
                checksum);
            db->execSqlSync("COMMIT");
            ++appliedCount;
            APP_LOG_INFO << "database migration applied: " << version;
        } catch (...) {
            try {
                db->execSqlSync("ROLLBACK");
            } catch (...) {
            }
            throw;
        }
    }

    APP_LOG_INFO << "database schema ready, baseStatements="
                 << schemaStatements.size()
                 << ", migrationsApplied=" << appliedCount
                 << ", migrationChecksumsBackfilled="
                 << checksumBackfillCount
                 << ", migrationsAvailable=" << migrations.size();
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
        if (!db)
            throw std::runtime_error("default DbClient is unavailable");
        try {
            configureDatabase(db);
            applySchemaAndMigrations(db);
        } catch (const std::exception& error) {
            APP_LOG_ERROR << "database migration failed: " << error.what();
            // 启动失败时异步日志线程可能来不及刷盘，stderr 保留可诊断信息。
            std::fprintf(
                stderr, "database migration failed: %s\n", error.what());
            std::fflush(stderr);
            throw;
        }
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
            if (stopping_) return;
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

    void shutdown() {
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (stopping_) return;
            stopping_ = true;
        }
        cv_.notify_one();
        if (worker_.joinable()) worker_.join();
    }

private:
    AsyncConsoleSink() {
        worker_ = std::thread([this] { loop(); });
    }

    ~AsyncConsoleSink() {
        shutdown();
    }

    void loop() {
        std::deque<std::string> local;
        for (;;) {
            {
                std::unique_lock<std::mutex> lk(mu_);
                cv_.wait(
                    lk,
                    [this] { return stopping_ || !queue_.empty(); });
                if (queue_.empty() && stopping_) break;
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
    bool                    stopping_{false};
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

static_assert(
    std::atomic<std::sig_atomic_t>::is_always_lock_free,
    "signal coordination requires lock-free sig_atomic_t");
std::atomic<std::sig_atomic_t> shutdownSignal{0};
std::atomic<bool> stopShutdownCoordinator{false};
std::thread shutdownCoordinator;

void recordShutdownSignal(std::sig_atomic_t signalCode) noexcept {
    // Drogon invokes this callback directly from the OS signal handler.
    // Keep it async-signal-safe: only use a lock-free atomic here.
    std::sig_atomic_t expected = 0;
    if (!shutdownSignal.compare_exchange_strong(
            expected,
            signalCode,
            std::memory_order_relaxed)) {
        shutdownSignal.store(3, std::memory_order_relaxed);
    }
}

void runShutdownCoordinator() {
    while (!stopShutdownCoordinator.load(std::memory_order_acquire)
           && shutdownSignal.load(std::memory_order_relaxed) == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (stopShutdownCoordinator.load(std::memory_order_acquire)) return;

    const auto firstSignal =
        shutdownSignal.load(std::memory_order_relaxed);
    const char* signalName = firstSignal == 2 ? "SIGINT" : "SIGTERM";
    auto& lifecycle = Lifecycle::instance();
    if (!lifecycle.beginDraining()) return;

    const auto graceSeconds = lifecycle.gracePeriodSeconds();
    APP_LOG_WARN << "graceful shutdown started signal=" << signalName
                 << " grace_seconds=" << graceSeconds
                 << " active_requests=" << lifecycle.activeRequests();

    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::seconds(graceSeconds);
    bool drained = lifecycle.activeRequests() == 0;
    while (!drained
           && shutdownSignal.load(std::memory_order_relaxed) != 3
           && std::chrono::steady_clock::now() < deadline) {
        drained = lifecycle.waitUntilDrained(
            std::chrono::milliseconds(50));
    }

    if (shutdownSignal.load(std::memory_order_relaxed) == 3) {
        APP_LOG_WARN << "second shutdown signal received; "
                        "forcing immediate stop";
    } else if (drained) {
        APP_LOG_INFO << "graceful shutdown drained all requests";
    } else {
        APP_LOG_WARN
            << "graceful shutdown deadline reached active_requests="
            << lifecycle.activeRequests();
    }
    lifecycle.markStopped();
    drogon::app().quit();
}

void startShutdownCoordinator() {
    stopShutdownCoordinator.store(false, std::memory_order_release);
    shutdownCoordinator = std::thread(runShutdownCoordinator);
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
    app.setTermSignalHandler(
        [] { recordShutdownSignal(1); });
    app.setIntSignalHandler(
        [] { recordShutdownSignal(2); });

    // CORS 必须覆盖 404/鉴权失败等所有响应，因此使用全局 advice，而非逐路由挂 Filter。
    app.registerSyncAdvice([](const drogon::HttpRequestPtr& req) {
        prepareRequest(req);
        if (Lifecycle::instance().isDraining()
            && req->getPath() != "/api/health"
            && req->getPath() != "/api/ready"
            && req->getPath() != "/api/metrics") {
            auto resp = Result::fail(
                5031,
                "service is shutting down",
                drogon::k503ServiceUnavailable);
            resp->addHeader("Connection", "close");
            resp->addHeader("Retry-After", "1");
            completeRequest(req, resp);
            return resp;
        }
        if (filters::CorsFilter::isPreflight(req)) {
            auto resp = filters::CorsFilter::handlePreflight(req);
            completeRequest(req, resp);
            return resp;
        }
        return drogon::HttpResponsePtr{};
    });
    app.registerPreSendingAdvice(
        [](const drogon::HttpRequestPtr& req,
           const drogon::HttpResponsePtr& resp) {
            filters::CorsFilter::applyResponseHeaders(req, resp);
            completeRequest(req, resp);
        });

    // 启动 banner
    app.registerBeginningAdvice([]() {
        startShutdownCoordinator();
        Lifecycle::instance().markReady();
        APP_LOG_INFO << "============================================";
        APP_LOG_INFO << "  drogon-admin started (Drogon + SQLite modular)";
        APP_LOG_INFO << "============================================";
    });
}

void Bootstrap::shutdown() {
    stopShutdownCoordinator.store(true, std::memory_order_release);
    if (shutdownCoordinator.joinable()) shutdownCoordinator.join();
    Lifecycle::instance().markStopped();
    APP_LOG_INFO << "drogon-admin stopped";
    getAsyncFileLogger().flush();
    AsyncConsoleSink::instance().shutdown();
    getAsyncFileLogger().flush();
}

} // namespace core
