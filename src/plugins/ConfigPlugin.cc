#include "ConfigPlugin.h"
#include "core/Lifecycle.h"
#include "core/Logger.h"
#include <drogon/drogon.h>
#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <sstream>
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

bool boolEnvOrDefault(const char* name, bool fallback) {
    auto value = envOrEmpty(name);
    if (value.empty()) return fallback;
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                   });
    if (value == "1" || value == "true" || value == "yes") return true;
    if (value == "0" || value == "false" || value == "no") return false;
    throw std::runtime_error(
        std::string(name) + " must be true or false");
}

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    value = value.substr(first, last - first + 1);
    if (value.size() > 8 && value.back() == '/') value.pop_back();
    return value;
}

bool validOrigin(const std::string& origin) {
    if (origin == "*") return true;
    const bool hasScheme =
        origin.rfind("http://", 0) == 0
        || origin.rfind("https://", 0) == 0;
    if (!hasScheme) return false;
    const auto schemeEnd = origin.find("://") + 3;
    if (schemeEnd >= origin.size()) return false;
    return origin.find_first_of("/?# \t\r\n", schemeEnd)
        == std::string::npos;
}

std::vector<std::string> parseOrigins(const std::string& value) {
    std::vector<std::string> origins;
    std::stringstream stream(value);
    std::string item;
    while (std::getline(stream, item, ',')) {
        item = trim(std::move(item));
        if (!item.empty()) origins.push_back(std::move(item));
    }
    return origins;
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

    if (custom.isMember("cors_allowed_origins")) {
        const auto& origins = custom["cors_allowed_origins"];
        if (!origins.isArray())
            throw std::runtime_error("cors_allowed_origins must be an array");
        for (const auto& origin : origins) {
            if (!origin.isString())
                throw std::runtime_error(
                    "cors_allowed_origins entries must be strings");
            corsAllowedOrigins_.push_back(trim(origin.asString()));
        }
    }
    const auto corsOriginsEnv = envOrEmpty("DROGON_CORS_ALLOWED_ORIGINS");
    if (!corsOriginsEnv.empty())
        corsAllowedOrigins_ = parseOrigins(corsOriginsEnv);
    if (custom.isMember("cors_allow_credentials"))
        corsAllowCredentials_ = custom["cors_allow_credentials"].asBool();
    if (custom.isMember("cors_max_age_seconds"))
        corsMaxAgeSeconds_ = custom["cors_max_age_seconds"].asInt();
    if (custom.isMember("enable_hsts"))
        hstsEnabled_ = custom["enable_hsts"].asBool();
    corsAllowCredentials_ = boolEnvOrDefault(
        "DROGON_CORS_ALLOW_CREDENTIALS", corsAllowCredentials_);
    corsMaxAgeSeconds_ = intEnvOrDefault(
        "DROGON_CORS_MAX_AGE_SECONDS", corsMaxAgeSeconds_);
    hstsEnabled_ = boolEnvOrDefault(
        "DROGON_ENABLE_HSTS", hstsEnabled_);
    if (custom.isMember("shutdown_grace_seconds"))
        shutdownGraceSeconds_ = custom["shutdown_grace_seconds"].asInt();
    shutdownGraceSeconds_ = intEnvOrDefault(
        "DROGON_SHUTDOWN_GRACE_SECONDS", shutdownGraceSeconds_);

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
    if (corsMaxAgeSeconds_ < 0 || corsMaxAgeSeconds_ > 86400)
        throw std::runtime_error(
            "cors_max_age_seconds must be between 0 and 86400");
    if (shutdownGraceSeconds_ < 1 || shutdownGraceSeconds_ > 300)
        throw std::runtime_error(
            "shutdown_grace_seconds must be between 1 and 300");

    std::sort(corsAllowedOrigins_.begin(), corsAllowedOrigins_.end());
    corsAllowedOrigins_.erase(
        std::unique(corsAllowedOrigins_.begin(), corsAllowedOrigins_.end()),
        corsAllowedOrigins_.end());
    for (const auto& origin : corsAllowedOrigins_) {
        if (!validOrigin(origin))
            throw std::runtime_error(
                "invalid CORS origin (expected exact http(s) origin): "
                + origin);
    }
    const bool wildcardCors = std::binary_search(
        corsAllowedOrigins_.begin(), corsAllowedOrigins_.end(),
        std::string("*"));
    if (wildcardCors && corsAllowCredentials_)
        throw std::runtime_error(
            "CORS wildcard cannot be combined with credentials");
    if (environment_ == "production" && wildcardCors)
        throw std::runtime_error(
            "CORS wildcard is not allowed in production");

    if (insecureSecret(jwtSecret_)) {
        if (environment_ == "production")
            throw std::runtime_error(
                "DROGON_JWT_SECRET must contain at least 32 characters in production");
        jwtSecret_ = randomSecret();
        APP_LOG_WARN
            << "using an ephemeral JWT secret in development; existing tokens "
               "will be invalid after restart";
    }

    core::Lifecycle::instance().configureGracePeriod(
        shutdownGraceSeconds_);
    APP_LOG_INFO << "ConfigPlugin loaded, environment=" << environment_
                 << ", jwtExpireSec=" << jwtExpireSec_
                 << ", registrationMode=" << registrationMode_
                 << ", loginMaxFailures=" << loginMaxFailures_
                 << ", loginLockSeconds=" << loginLockSeconds_
                 << ", corsOrigins=" << corsAllowedOrigins_.size()
                 << ", corsCredentials=" << corsAllowCredentials_
                 << ", hsts=" << hstsEnabled_
                 << ", shutdownGraceSeconds=" << shutdownGraceSeconds_;
    if (environment_ == "production" && !hstsEnabled_) {
        APP_LOG_WARN
            << "HSTS is disabled; enable it only when every public request "
               "is served through HTTPS";
    }
}

void ConfigPlugin::shutdown() {
    APP_LOG_INFO << "ConfigPlugin shutdown";
}

bool ConfigPlugin::isCorsOriginAllowed(std::string_view origin) const {
    auto normalized = trim(std::string(origin));
    return std::binary_search(
               corsAllowedOrigins_.begin(), corsAllowedOrigins_.end(),
               std::string("*"))
        || std::binary_search(
               corsAllowedOrigins_.begin(), corsAllowedOrigins_.end(),
               normalized);
}

} // namespace plugins
