#pragma once
//
// Crypto 工具：
//   - base64Url(Encode/Decode)
//   - Argon2id 密码哈希（随机盐）与旧 SHA-256 哈希的渐进迁移
//   - hmacSha256（JWT 签名用）
//
#include <argon2.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace common {

class CryptoUtil {
public:
    // -- base64 url-safe encode / decode ------------------------------------
    static std::string base64UrlEncode(const void* data, size_t len) {
        static const char kAlphabet[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        const auto* p = static_cast<const uint8_t*>(data);
        std::string out;
        out.reserve(((len + 2) / 3) * 4);

        size_t i = 0;
        while (i + 3 <= len) {
            uint32_t v = (uint32_t(p[i]) << 16) | (uint32_t(p[i + 1]) << 8) | p[i + 2];
            out.push_back(kAlphabet[(v >> 18) & 0x3F]);
            out.push_back(kAlphabet[(v >> 12) & 0x3F]);
            out.push_back(kAlphabet[(v >> 6) & 0x3F]);
            out.push_back(kAlphabet[v & 0x3F]);
            i += 3;
        }
        if (i < len) {
            uint32_t v = uint32_t(p[i]) << 16;
            if (i + 1 < len) v |= uint32_t(p[i + 1]) << 8;
            out.push_back(kAlphabet[(v >> 18) & 0x3F]);
            out.push_back(kAlphabet[(v >> 12) & 0x3F]);
            if (i + 1 < len)
                out.push_back(kAlphabet[(v >> 6) & 0x3F]);
        }
        return out;
    }

    static std::string base64UrlEncode(std::string_view s) {
        return base64UrlEncode(s.data(), s.size());
    }

    static bool base64UrlDecode(std::string_view in, std::string& out) {
        static int8_t table[256];
        static bool   inited = false;
        if (!inited) {
            for (auto& v : table) v = -1;
            const char* kAlphabet =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
            for (int i = 0; i < 64; ++i)
                table[static_cast<uint8_t>(kAlphabet[i])] = static_cast<int8_t>(i);
            inited = true;
        }
        out.clear();
        out.reserve(in.size() * 3 / 4 + 2);
        uint32_t buf = 0;
        int bits = 0;
        for (char c : in) {
            if (c == '=') continue;
            int v = table[static_cast<uint8_t>(c)];
            if (v < 0) return false;
            buf = (buf << 6) | static_cast<uint32_t>(v);
            bits += 6;
            if (bits >= 8) {
                bits -= 8;
                out.push_back(static_cast<char>((buf >> bits) & 0xFF));
            }
        }
        return true;
    }

    // -- sha256 hex ----------------------------------------------------------
    static std::string sha256Hex(std::string_view data) {
        std::array<uint8_t, SHA256_DIGEST_LENGTH> digest{};
        SHA256(reinterpret_cast<const uint8_t*>(data.data()), data.size(),
               digest.data());
        return toHex(digest.data(), digest.size());
    }

    // 密码哈希：Argon2id v1.3，64 MiB 内存、3 次迭代、16 字节随机盐。
    static std::string hashPassword(std::string_view password) {
        std::array<uint8_t, kArgonSaltLength> salt{};
        if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1)
            throw std::runtime_error("secure random generation failed");

        const auto encodedLen = argon2_encodedlen(
            kArgonTimeCost, kArgonMemoryCost, kArgonParallelism,
            kArgonSaltLength, kArgonHashLength, Argon2_id);
        std::vector<char> encoded(encodedLen);
        const int rc = argon2id_hash_encoded(
            kArgonTimeCost, kArgonMemoryCost, kArgonParallelism,
            password.data(), password.size(),
            salt.data(), salt.size(), kArgonHashLength,
            encoded.data(), encoded.size());
        if (rc != ARGON2_OK)
            throw std::runtime_error(
                std::string("argon2id hash failed: ") + argon2_error_message(rc));
        return encoded.data();
    }

    static bool verifyPassword(std::string_view password,
                               std::string_view storedHash) {
        if (storedHash.rfind("$argon2id$", 0) == 0) {
            const std::string encoded(storedHash);
            return argon2id_verify(encoded.c_str(),
                                   password.data(), password.size()) == ARGON2_OK;
        }

        // 兼容旧库：验证成功后由 UserService 立即升级为 Argon2id。
        const auto legacy = legacyPasswordHash(password);
        return legacy.size() == storedHash.size()
            && CRYPTO_memcmp(legacy.data(), storedHash.data(), legacy.size()) == 0;
    }

    // 用户不存在时仍执行同成本的 Argon2id 校验，减少登录邮箱枚举时序差异。
    static void performDummyPasswordCheck(std::string_view password) {
        (void)verifyPassword(password, kDummyPasswordHash);
    }

    static bool passwordNeedsRehash(std::string_view storedHash) {
        if (storedHash.rfind("$argon2id$", 0) != 0) return true;
        return storedHash.find("$m=65536,t=3,p=1$") == std::string_view::npos;
    }

    // -- hmac sha256 ---------------------------------------------------------
    static std::string hmacSha256Raw(std::string_view key,
                                     std::string_view data) {
        unsigned int len = 0;
        uint8_t out[EVP_MAX_MD_SIZE];
        HMAC(EVP_sha256(),
             key.data(), static_cast<int>(key.size()),
             reinterpret_cast<const uint8_t*>(data.data()), data.size(),
             out, &len);
        return std::string(reinterpret_cast<char*>(out), len);
    }

private:
    static constexpr uint32_t kArgonTimeCost = 3;
    static constexpr uint32_t kArgonMemoryCost = 64 * 1024;
    static constexpr uint32_t kArgonParallelism = 1;
    static constexpr uint32_t kArgonSaltLength = 16;
    static constexpr uint32_t kArgonHashLength = 32;
    static constexpr const char* kLegacyPasswordSalt = "drogon_admin_salt_v1";
    static constexpr const char* kDummyPasswordHash =
        "$argon2id$v=19$m=65536,t=3,p=1$"
        "GmPQlNEtYLOZ2dUBXqMtlQ$"
        "sZ+Z352rSnDfmJSjetdIE2A9nqP6PApE3LhGRebePoA";

    static std::string legacyPasswordHash(std::string_view password) {
        std::string value = kLegacyPasswordSalt;
        value.append(password.data(), password.size());
        return sha256Hex(value);
    }

    static std::string toHex(const uint8_t* data, size_t len) {
        static const char* kHex = "0123456789abcdef";
        std::string s;
        s.resize(len * 2);
        for (size_t i = 0; i < len; ++i) {
            s[i * 2]     = kHex[(data[i] >> 4) & 0xF];
            s[i * 2 + 1] = kHex[data[i] & 0xF];
        }
        return s;
    }
};

} // namespace common
