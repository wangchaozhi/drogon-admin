#include "MenuController.h"
#include "RbacService.h"
#include "dto/RbacReq.h"
#include "core/Result.h"
#include "filters/PermissionRegistry.h"

namespace modules::rbac {

MenuController::MenuController() {
    auto& reg = filters::PermissionRegistry::instance();
    reg.bind("GET",    "/api/menus",              "menu:view");
    reg.bind("GET",    "/api/menus/tree",         "menu:view");
    reg.bind("POST",   "/api/menus",              "menu:create");
    reg.bind("PUT",    "/api/menus/(\\d+)",       "menu:update");
    reg.bind("DELETE", "/api/menus/(\\d+)",       "menu:delete");
}

drogon::AsyncTask MenuController::list(
    drogon::HttpRequestPtr /*req*/,
    std::function<void(const drogon::HttpResponsePtr&)> cb) {
    try {
        auto items = co_await RbacService::instance().repo().listMenus();
        Json::Value arr(Json::arrayValue);
        for (const auto& m : items) arr.append(m.toJson());
        cb(core::Result::ok(arr));
    } catch (const std::exception& e) {
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
    }
}

drogon::AsyncTask MenuController::tree(
    drogon::HttpRequestPtr /*req*/,
    std::function<void(const drogon::HttpResponsePtr&)> cb) {
    try {
        auto roots = co_await RbacService::instance().repo().menuTree();
        Json::Value arr(Json::arrayValue);
        for (const auto& m : roots) arr.append(m.toJson());
        cb(core::Result::ok(arr));
    } catch (const std::exception& e) {
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
    }
}

drogon::AsyncTask MenuController::create(
    drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr&)> cb) {
    auto json = req->getJsonObject();
    if (!json) { cb(core::Result::fail(4001, "invalid json body")); co_return; }
    std::string err;
    auto m = dto::MenuUpsertReq::parse(*json, err);
    if (!m) { cb(core::Result::fail(4002, err)); co_return; }
    try {
        auto saved = co_await RbacService::instance().repo().createMenu(*m);
        RbacService::instance().invalidateAll();
        cb(core::Result::ok(saved.toJson(), "created"));
    } catch (const std::exception& e) {
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
    }
}

drogon::AsyncTask MenuController::update(
    drogon::HttpRequestPtr req,
    std::function<void(const drogon::HttpResponsePtr&)> cb,
    int64_t id) {
    auto json = req->getJsonObject();
    if (!json) { cb(core::Result::fail(4001, "invalid json body")); co_return; }
    std::string err;
    auto m = dto::MenuUpsertReq::parse(*json, err);
    if (!m) { cb(core::Result::fail(4002, err)); co_return; }
    try {
        bool ok = co_await RbacService::instance().repo().updateMenu(id, *m);
        if (!ok) {
            cb(core::Result::notFound("menu not found"));
            co_return;
        }
        RbacService::instance().invalidateAll();
        auto saved = co_await RbacService::instance().repo().getMenu(id);
        cb(core::Result::ok(saved ? saved->toJson() : Json::nullValue, "updated"));
    } catch (const std::exception& e) {
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
    }
}

drogon::AsyncTask MenuController::remove(
    drogon::HttpRequestPtr /*req*/,
    std::function<void(const drogon::HttpResponsePtr&)> cb,
    int64_t id) {
    try {
        bool ok = co_await RbacService::instance().repo().deleteMenu(id);
        if (!ok) {
            cb(core::Result::notFound("menu not found"));
            co_return;
        }
        RbacService::instance().invalidateAll();
        cb(core::Result::ok(Json::nullValue, "deleted"));
    } catch (const std::exception& e) {
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
    }
}

} // namespace modules::rbac
