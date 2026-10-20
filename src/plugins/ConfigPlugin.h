#pragma once
//
// 配置插件：把 custom_config 区块包装成全局单例，供业务读取。
// 通过 drogon::app().getPlugin<plugins::ConfigPlugin>() 获取实例。
//
#include <drogon/plugins/Plugin.h>
#include <json/json.h>
#include <string>
#include <string_view>
#include <vector>

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
    int loginRateLimitGlobal() const { return loginRateLimitGlobal_; }
    int loginRateLimitPerIdentifier() const {
        return loginRateLimitPerIdentifier_;
    }
    int loginRateLimitWindowSeconds() const {
        return loginRateLimitWindowSeconds_;
    }
    bool isCorsOriginAllowed(std::string_view origin) const;
    bool corsAllowCredentials() const { return corsAllowCredentials_; }
    int corsMaxAgeSeconds() const { return corsMaxAgeSeconds_; }
    bool hstsEnabled() const { return hstsEnabled_; }
    int shutdownGraceSeconds() const { return shutdownGraceSeconds_; }

private:
    std::string jwtSecret_;
    int jwtExpireSec_{7200};
    std::string environment_{"development"};
    std::string registrationMode_{"first-user"};
    int loginMaxFailures_{5};
    int loginLockSeconds_{900};
    int loginRateLimitGlobal_{120};
    int loginRateLimitPerIdentifier_{10};
    int loginRateLimitWindowSeconds_{60};
    std::vector<std::string> corsAllowedOrigins_;
    bool corsAllowCredentials_{false};
    int corsMaxAgeSeconds_{600};
    bool hstsEnabled_{false};
    int shutdownGraceSeconds_{10};
};

} // namespace plugins
