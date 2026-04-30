#include "ConfigPlugin.h"
#include "core/Logger.h"
#include <drogon/drogon.h>

namespace plugins {

void ConfigPlugin::initAndStart(const Json::Value& /*pluginCfg*/) {
    // Drogon 的 plugin 独有配置走形参；此处我们复用全局 custom_config。
    const auto& custom = drogon::app().getCustomConfig();
    if (custom.isMember("jwt_secret"))
        jwtSecret_ = custom["jwt_secret"].asString();
    if (custom.isMember("jwt_expire_seconds"))
        jwtExpireSec_ = custom["jwt_expire_seconds"].asInt();
    if (custom.isMember("super_admin_email"))
        superAdminEmail_ = custom["super_admin_email"].asString();

    APP_LOG_INFO << "ConfigPlugin loaded, jwtExpireSec=" << jwtExpireSec_
                 << ", superAdmin=" << superAdminEmail_;
}

void ConfigPlugin::shutdown() {
    APP_LOG_INFO << "ConfigPlugin shutdown";
}

} // namespace plugins
