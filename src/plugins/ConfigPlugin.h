#pragma once
//
// 配置插件：把 custom_config 区块包装成全局单例，供业务读取。
// 通过 drogon::app().getPlugin<plugins::ConfigPlugin>() 获取实例。
//
#include <drogon/plugins/Plugin.h>
#include <json/json.h>
#include <string>

namespace plugins {

class ConfigPlugin : public drogon::Plugin<ConfigPlugin> {
public:
    ConfigPlugin() = default;

    void initAndStart(const Json::Value& config) override;
    void shutdown() override;

    const std::string& jwtSecret() const { return jwtSecret_; }
    int jwtExpireSeconds() const { return jwtExpireSec_; }
    const std::string& superAdminEmail() const { return superAdminEmail_; }

private:
    std::string jwtSecret_{"change-me"};
    int jwtExpireSec_{7200};
    std::string superAdminEmail_{"admin@c_web.local"};
};

} // namespace plugins
