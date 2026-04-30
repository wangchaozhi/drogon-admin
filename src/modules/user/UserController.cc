#include "UserController.h"
#include "core/Result.h"
#include "core/Logger.h"
#include "dto/CreateUserReq.h"
#include "dto/LoginReq.h"
#include "common/TimeUtil.h"

namespace modules::user {

namespace {
using CbPtr = std::shared_ptr<std::function<void(const drogon::HttpResponsePtr&)>>;

CbPtr shareCb(std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    return std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(cb));
}
} // namespace

void UserController::getById(const drogon::HttpRequestPtr& /*req*/,
                             std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                             int64_t id) {
    APP_LOG_DEBUG << "getById id=" << id;
    auto callback = shareCb(std::move(cb));

    svc_.getById(
        id,
        [callback](std::optional<dto::UserDto> u) {
            if (!u) { (*callback)(core::Result::notFound("user not found")); return; }
            (*callback)(core::Result::ok(u->toJson()));
        },
        [callback](const std::string& err) {
            (*callback)(core::Result::fail(5001, "db error: " + err,
                                           drogon::k500InternalServerError));
        });
}

void UserController::registerUser(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    auto callback = shareCb(std::move(cb));

    auto json = req->getJsonObject();
    if (!json) {
        (*callback)(core::Result::fail(4001, "invalid json body"));
        return;
    }
    std::string err;
    auto reqDto = dto::CreateUserReq::parse(*json, err);
    if (!reqDto) {
        (*callback)(core::Result::fail(4002, err));
        return;
    }

    svc_.create(
        *reqDto,
        [callback](dto::UserDto u) {
            APP_LOG_INFO << "user created id=" << u.id;
            (*callback)(core::Result::ok(u.toJson(), "created"));
        },
        [callback](const std::string& e) {
            // 唯一约束冲突 → 邮箱已占用
            int code = (e.find("UNIQUE") != std::string::npos ||
                        e.find("unique") != std::string::npos) ? 4090 : 5002;
            auto http = (code == 4090) ? drogon::k409Conflict
                                       : drogon::k500InternalServerError;
            std::string msg = (code == 4090) ? "email already registered"
                                             : "db error: " + e;
            (*callback)(core::Result::fail(code, msg, http));
        });
}

void UserController::login(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    auto callback = shareCb(std::move(cb));

    auto json = req->getJsonObject();
    if (!json) {
        (*callback)(core::Result::fail(4001, "invalid json body"));
        return;
    }
    std::string err;
    auto reqDto = dto::LoginReq::parse(*json, err);
    if (!reqDto) {
        (*callback)(core::Result::fail(4002, err));
        return;
    }

    svc_.login(
        *reqDto,
        [callback](LoginResult r) {
            Json::Value data;
            data["token"]      = r.token;
            data["token_type"] = "Bearer";
            data["expires_at"] = static_cast<Json::Int64>(r.expiresAt);
            data["user"]       = r.user.toJson();
            APP_LOG_INFO << "user login id=" << r.user.id;
            (*callback)(core::Result::ok(std::move(data), "login ok"));
        },
        [callback](const std::string& msg) {
            (*callback)(core::Result::fail(4011, msg, drogon::k401Unauthorized));
        },
        [callback](const std::string& e) {
            (*callback)(core::Result::fail(5003, "db error: " + e,
                                           drogon::k500InternalServerError));
        });
}

void UserController::me(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    auto callback = shareCb(std::move(cb));

    // AuthFilter 已将 userId / email 写入 attributes
    auto attrs = req->attributes();
    if (!attrs || !attrs->find("userId")) {
        (*callback)(core::Result::unauthorized("missing auth context"));
        return;
    }
    int64_t uid = attrs->get<int64_t>("userId");

    svc_.getById(
        uid,
        [callback](std::optional<dto::UserDto> u) {
            if (!u) { (*callback)(core::Result::notFound("user not found")); return; }
            (*callback)(core::Result::ok(u->toJson()));
        },
        [callback](const std::string& err) {
            (*callback)(core::Result::fail(5001, "db error: " + err,
                                           drogon::k500InternalServerError));
        });
}

void UserController::health(const drogon::HttpRequestPtr& /*req*/,
                            std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    Json::Value data;
    data["status"] = "ok";
    data["time"]   = common::TimeUtil::formatLocal(common::TimeUtil::nowSec());
    cb(core::Result::ok(std::move(data)));
}

} // namespace modules::user
