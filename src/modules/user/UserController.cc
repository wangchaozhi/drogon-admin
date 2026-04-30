#include "UserController.h"
#include "core/Result.h"
#include "core/Logger.h"
#include "dto/CreateUserReq.h"
#include "common/TimeUtil.h"

namespace modules::user {

void UserController::getById(const drogon::HttpRequestPtr& req,
                             std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                             int64_t id) {
    APP_LOG_DEBUG << "getById id=" << id;
    // 使用 shared_ptr 包住 cb 以便在多个 lambda 间共享
    auto callback = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(cb));

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

void UserController::create(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    auto callback = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(cb));

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
            (*callback)(core::Result::fail(5002, "db error: " + e,
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
