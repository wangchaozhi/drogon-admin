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

void MenuController::list(const drogon::HttpRequestPtr&,
                          std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    try {
        auto items = RbacService::instance().repo().listMenus();
        Json::Value arr(Json::arrayValue);
        for (const auto& m : items) arr.append(m.toJson());
        cb(core::Result::ok(arr));
    } catch (const std::exception& e) {
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
    }
}

void MenuController::tree(const drogon::HttpRequestPtr&,
                          std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    try {
        auto roots = RbacService::instance().repo().menuTree();
        Json::Value arr(Json::arrayValue);
        for (const auto& m : roots) arr.append(m.toJson());
        cb(core::Result::ok(arr));
    } catch (const std::exception& e) {
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
    }
}

void MenuController::create(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
    auto json = req->getJsonObject();
    if (!json) { cb(core::Result::fail(4001, "invalid json body")); return; }
    std::string err;
    auto m = dto::MenuUpsertReq::parse(*json, err);
    if (!m) { cb(core::Result::fail(4002, err)); return; }
    try {
        auto saved = RbacService::instance().repo().createMenu(*m);
        RbacService::instance().invalidateAll();
        cb(core::Result::ok(saved.toJson(), "created"));
    } catch (const std::exception& e) {
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
    }
}

void MenuController::update(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                            int64_t id) {
    auto json = req->getJsonObject();
    if (!json) { cb(core::Result::fail(4001, "invalid json body")); return; }
    std::string err;
    auto m = dto::MenuUpsertReq::parse(*json, err);
    if (!m) { cb(core::Result::fail(4002, err)); return; }
    try {
        if (!RbacService::instance().repo().updateMenu(id, *m)) {
            cb(core::Result::notFound("menu not found"));
            return;
        }
        RbacService::instance().invalidateAll();
        auto saved = RbacService::instance().repo().getMenu(id);
        cb(core::Result::ok(saved ? saved->toJson() : Json::nullValue, "updated"));
    } catch (const std::exception& e) {
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
    }
}

void MenuController::remove(const drogon::HttpRequestPtr&,
                            std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                            int64_t id) {
    try {
        if (!RbacService::instance().repo().deleteMenu(id)) {
            cb(core::Result::notFound("menu not found"));
            return;
        }
        RbacService::instance().invalidateAll();
        cb(core::Result::ok(Json::nullValue, "deleted"));
    } catch (const std::exception& e) {
        cb(core::Result::fail(5003, std::string("db error: ") + e.what(),
                              drogon::k500InternalServerError));
    }
}

} // namespace modules::rbac
