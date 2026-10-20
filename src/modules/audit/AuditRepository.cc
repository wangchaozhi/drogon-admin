#include "AuditRepository.h"
#include "common/TimeUtil.h"
#include <drogon/drogon.h>
#include <sstream>

namespace modules::audit {

namespace {

Json::Value parseDetails(const std::string& text) {
    Json::CharReaderBuilder builder;
    Json::Value value;
    std::string errors;
    std::istringstream stream(text);
    if (!Json::parseFromStream(builder, stream, &value, &errors)
        || !value.isObject()) {
        Json::Value fallback(Json::objectValue);
        fallback["unavailable"] = true;
        return fallback;
    }
    return value;
}

dto::AuditLogDto fromRow(const drogon::orm::Row& row) {
    dto::AuditLogDto item;
    item.id = row["id"].as<int64_t>();
    item.actorUserId = row["actor_user_id"].as<int64_t>();
    item.action = row["action"].as<std::string>();
    item.targetType = row["target_type"].as<std::string>();
    item.targetId = row["target_id"].as<std::string>();
    item.requestId = row["request_id"].as<std::string>();
    item.details = parseDetails(row["details"].as<std::string>());
    item.createdAt = row["created_at"].as<int64_t>();
    return item;
}

} // namespace

drogon::orm::DbClientPtr AuditRepository::db() const {
    return drogon::app().getDbClient();
}

drogon::Task<> AuditRepository::insert(
    int64_t actorUserId,
    std::string action,
    std::string targetType,
    std::string targetId,
    std::string requestId,
    Json::Value details) {
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    const auto serialized = Json::writeString(writer, details);
    co_await db()->execSqlCoro(
        "INSERT INTO audit_logs("
        "actor_user_id,action,target_type,target_id,request_id,details,created_at"
        ") VALUES(?,?,?,?,?,?,?)",
        actorUserId,
        std::move(action),
        std::move(targetType),
        std::move(targetId),
        std::move(requestId),
        serialized,
        common::TimeUtil::nowSec());
    co_return;
}

drogon::Task<dto::AuditLogPage> AuditRepository::listPaged(
    int page,
    int pageSize,
    std::string action) {
    dto::AuditLogPage result;
    const int64_t offset =
        static_cast<int64_t>(page - 1) * pageSize;

    drogon::orm::Result count;
    drogon::orm::Result rows;
    if (action.empty()) {
        count = co_await db()->execSqlCoro(
            "SELECT COUNT(*) AS c FROM audit_logs");
        rows = co_await db()->execSqlCoro(
            "SELECT id,actor_user_id,action,target_type,target_id,"
            "request_id,details,created_at "
            "FROM audit_logs ORDER BY created_at DESC,id DESC LIMIT ? OFFSET ?",
            pageSize,
            offset);
    } else {
        count = co_await db()->execSqlCoro(
            "SELECT COUNT(*) AS c FROM audit_logs WHERE action=?",
            action);
        rows = co_await db()->execSqlCoro(
            "SELECT id,actor_user_id,action,target_type,target_id,"
            "request_id,details,created_at "
            "FROM audit_logs WHERE action=? "
            "ORDER BY created_at DESC,id DESC LIMIT ? OFFSET ?",
            action,
            pageSize,
            offset);
    }

    result.total = count.empty() ? 0 : count[0]["c"].as<int64_t>();
    result.items.reserve(rows.size());
    for (const auto& row : rows)
        result.items.push_back(fromRow(row));
    co_return result;
}

} // namespace modules::audit
