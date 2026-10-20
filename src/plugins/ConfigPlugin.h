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
    const std::string& environment() const { return environment_; }
    const std::string& registrationMode() const { return registrationMode_; }
    int loginMaxFailures() const { return loginMaxFailures_; }
    int loginLockSeconds() const { return loginLockSeconds_; }

private:
    std::string jwtSecret_;
    int jwtExpireSec_{7200};
    std::string environment_{"development"};
    std::string registrationMode_{"first-user"};
    int loginMaxFailures_{5};
    int loginLockSeconds_{900};
};

} // namespace plugins
