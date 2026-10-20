#include "ConfigPlugin.h"
#include "core/Logger.h"
#include <drogon/drogon.h>
#include <openssl/rand.h>

#include <array>
#include <cstdlib>
#include <stdexcept>

namespace plugins {

namespace {

std::string envOrEmpty(const char* name) {
    const char* value = std::getenv(name);
    return value ? std::string(value) : std::string();
}

int intEnvOrDefault(const char* name, int fallback) {
    const auto value = envOrEmpty(name);
    if (value.empty()) return fallback;
    try {
        size_t parsed = 0;
        const int result = std::stoi(value, &parsed);
        if (parsed != value.size()) throw std::invalid_argument("trailing characters");
        return result;
    } catch (const std::exception&) {
        throw std::runtime_error(std::string(name) + " must be an integer");
    }
}

std::string randomSecret() {
    std::array<unsigned char, 32> bytes{};
    if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1)
        throw std::runtime_error("failed to generate development JWT secret");
    static constexpr char hex[] = "0123456789abcdef";
    std::string out(bytes.size() * 2, '\0');
    for (size_t i = 0; i < bytes.size(); ++i) {
        out[i * 2] = hex[bytes[i] >> 4];
        out[i * 2 + 1] = hex[bytes[i] & 0x0f];
    }
    return out;
}

bool insecureSecret(const std::string& secret) {
    return secret.size() < 32
        || secret == "change-me"
        || secret == "change-me-in-production";
}

} // namespace

void ConfigPlugin::initAndStart(const Json::Value& /*pluginCfg*/) {
    // Drogon 的 plugin 独有配置走形参；此处我们复用全局 custom_config。
    const auto& custom = drogon::app().getCustomConfig();

    environment_ = envOrEmpty("DROGON_ENV");
    if (environment_.empty() && custom.isMember("environment"))
        environment_ = custom["environment"].asString();
    if (environment_.empty()) environment_ = "development";

    jwtSecret_ = envOrEmpty("DROGON_JWT_SECRET");
    if (jwtSecret_.empty() && custom.isMember("jwt_secret"))
        jwtSecret_ = custom["jwt_secret"].asString();

    if (custom.isMember("jwt_expire_seconds"))
        jwtExpireSec_ = custom["jwt_expire_seconds"].asInt();
    registrationMode_ = envOrEmpty("DROGON_REGISTRATION_MODE");
    if (registrationMode_.empty() && custom.isMember("registration_mode"))
        registrationMode_ = custom["registration_mode"].asString();
    if (registrationMode_.empty()) registrationMode_ = "first-user";
    if (custom.isMember("login_max_failures"))
        loginMaxFailures_ = custom["login_max_failures"].asInt();
    if (custom.isMember("login_lock_seconds"))
        loginLockSeconds_ = custom["login_lock_seconds"].asInt();
    loginMaxFailures_ = intEnvOrDefault(
        "DROGON_LOGIN_MAX_FAILURES", loginMaxFailures_);
    loginLockSeconds_ = intEnvOrDefault(
        "DROGON_LOGIN_LOCK_SECONDS", loginLockSeconds_);

    if (jwtExpireSec_ < 300 || jwtExpireSec_ > 86400)
        throw std::runtime_error(
            "jwt_expire_seconds must be between 300 and 86400");
    if (registrationMode_ != "disabled"
        && registrationMode_ != "first-user"
        && registrationMode_ != "open") {
        throw std::runtime_error(
            "registration_mode must be disabled, first-user, or open");
    }
    if (loginMaxFailures_ < 3 || loginMaxFailures_ > 20)
        throw std::runtime_error(
            "login_max_failures must be between 3 and 20");
    if (loginLockSeconds_ < 30 || loginLockSeconds_ > 86400)
        throw std::runtime_error(
            "login_lock_seconds must be between 30 and 86400");

    if (insecureSecret(jwtSecret_)) {
        if (environment_ == "production")
            throw std::runtime_error(
                "DROGON_JWT_SECRET must contain at least 32 characters in production");
        jwtSecret_ = randomSecret();
        APP_LOG_WARN
            << "using an ephemeral JWT secret in development; existing tokens "
               "will be invalid after restart";
    }

    APP_LOG_INFO << "ConfigPlugin loaded, environment=" << environment_
                 << ", jwtExpireSec=" << jwtExpireSec_
                 << ", registrationMode=" << registrationMode_
                 << ", loginMaxFailures=" << loginMaxFailures_
                 << ", loginLockSeconds=" << loginLockSeconds_;
}

void ConfigPlugin::shutdown() {
    APP_LOG_INFO << "ConfigPlugin shutdown";
}

} // namespace plugins
