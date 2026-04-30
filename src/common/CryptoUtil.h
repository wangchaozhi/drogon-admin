#pragma once
//
// Crypto 工具：
//   - base64Url(Encode/Decode)
//   - sha256Hex（用于密码哈希，加了固定盐；生产建议换 bcrypt/argon2）
//   - hmacSha256（JWT 签名用）
//
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace common {

class CryptoUtil {
public:
    // 固定盐，避免简单彩虹表；生产应为每用户独立盐
    static constexpr const char* kPasswordSalt = "drogon_admin_salt_v1";

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

    // 密码哈希：sha256(salt || password)
    static std::string hashPassword(std::string_view password) {
        std::string buf = kPasswordSalt;
        buf.append(password.data(), password.size());
        return sha256Hex(buf);
    }

    static bool verifyPassword(std::string_view password,
                               std::string_view storedHash) {
        auto h = hashPassword(password);
        if (h.size() != storedHash.size()) return false;
        // 常量时间比较
        unsigned diff = 0;
        for (size_t i = 0; i < h.size(); ++i)
            diff |= static_cast<unsigned>(h[i] ^ storedHash[i]);
        return diff == 0;
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
