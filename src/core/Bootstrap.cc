#include "Bootstrap.h"
#include "Logger.h"
#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

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
    // 确保 data 目录存在（sqlite filename 相对路径位于此）
    std::error_code ec;
    std::filesystem::create_directories("data", ec);

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
                APP_LOG_ERROR << "schema stmt failed: " << e.what()
                              << " | sql=" << s;
            }
        }
        APP_LOG_INFO << "schema.sql applied, statements=" << stmts.size();
    });
}

} // namespace

void Bootstrap::init(const std::string& configPath) {
    auto& app = drogon::app();
    app.loadConfigFile(configPath);

    ensureDbDirAndSchema();

    // 启动 banner
    app.registerBeginningAdvice([]() {
        APP_LOG_INFO << "============================================";
        APP_LOG_INFO << "  c_web started (Drogon + SQLite modular)";
        APP_LOG_INFO << "============================================";
    });
}

} // namespace core
