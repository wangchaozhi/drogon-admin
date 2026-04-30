#pragma once
//
// 轻量 JWT（HS256）实现：sign / verify。
// Payload 包含 sub(userId)、email、iat、exp。
// 只依赖 OpenSSL + JsonCpp，避免引入额外 jwt-cpp 依赖。
//
#include "CryptoUtil.h"
#include "TimeUtil.h"

#include <json/json.h>
#include <json/reader.h>
#include <json/writer.h>

#include <optional>
#include <string>
#include <string_view>

namespace common {

struct JwtPayload {
    int64_t     userId{0};
    std::string email;
    int64_t     iat{0};
    int64_t     exp{0};
};

class JwtUtil {
public:
    // 生成 token；secret 建议 >= 32 字节，expireSec 为有效期（秒）
    static std::string sign(int64_t userId,
                            const std::string& email,
                            const std::string& secret,
                            int expireSec) {
        int64_t now = common::TimeUtil::nowSec();

        Json::Value header;
        header["alg"] = "HS256";
        header["typ"] = "JWT";

        Json::Value payload;
        payload["sub"]   = static_cast<Json::Int64>(userId);
        payload["email"] = email;
        payload["iat"]   = static_cast<Json::Int64>(now);
        payload["exp"]   = static_cast<Json::Int64>(now + expireSec);

        Json::StreamWriterBuilder wb;
        wb["indentation"] = "";
        std::string headerStr  = Json::writeString(wb, header);
        std::string payloadStr = Json::writeString(wb, payload);

        std::string signingInput =
            CryptoUtil::base64UrlEncode(headerStr) + "." +
            CryptoUtil::base64UrlEncode(payloadStr);

        std::string sig = CryptoUtil::hmacSha256Raw(secret, signingInput);
        return signingInput + "." + CryptoUtil::base64UrlEncode(sig);
    }

    // 校验 token，成功返回 Payload；失败返回 nullopt 并填入 err
    static std::optional<JwtPayload> verify(std::string_view token,
                                            const std::string& secret,
                                            std::string& err) {
        // 切分 a.b.c
        auto dot1 = token.find('.');
        if (dot1 == std::string_view::npos) { err = "malformed token"; return std::nullopt; }
        auto dot2 = token.find('.', dot1 + 1);
        if (dot2 == std::string_view::npos) { err = "malformed token"; return std::nullopt; }

        std::string_view headerB64  = token.substr(0, dot1);
        std::string_view payloadB64 = token.substr(dot1 + 1, dot2 - dot1 - 1);
        std::string_view sigB64     = token.substr(dot2 + 1);

        // 重算签名
        std::string signingInput;
        signingInput.reserve(headerB64.size() + payloadB64.size() + 1);
        signingInput.append(headerB64);
        signingInput.push_back('.');
        signingInput.append(payloadB64);

        std::string expectedSig = CryptoUtil::hmacSha256Raw(secret, signingInput);
        std::string expectedSigB64 = CryptoUtil::base64UrlEncode(expectedSig);
        if (expectedSigB64.size() != sigB64.size() ||
            !constEq(expectedSigB64, sigB64)) {
            err = "bad signature";
            return std::nullopt;
        }

        // 解码 payload
        std::string payloadJson;
        if (!CryptoUtil::base64UrlDecode(payloadB64, payloadJson)) {
            err = "bad payload b64";
            return std::nullopt;
        }
        Json::Value j;
        Json::CharReaderBuilder rb;
        std::string parseErr;
        auto reader = std::unique_ptr<Json::CharReader>(rb.newCharReader());
        if (!reader->parse(payloadJson.data(),
                           payloadJson.data() + payloadJson.size(),
                           &j, &parseErr)) {
            err = "bad payload json: " + parseErr;
            return std::nullopt;
        }

        JwtPayload p;
        p.userId = j.get("sub", 0).asInt64();
        p.email  = j.get("email", "").asString();
        p.iat    = j.get("iat", 0).asInt64();
        p.exp    = j.get("exp", 0).asInt64();

        if (p.exp > 0 && common::TimeUtil::nowSec() >= p.exp) {
            err = "token expired";
            return std::nullopt;
        }
        return p;
    }

private:
    static bool constEq(std::string_view a, std::string_view b) {
        if (a.size() != b.size()) return false;
        unsigned diff = 0;
        for (size_t i = 0; i < a.size(); ++i)
            diff |= static_cast<unsigned>(a[i] ^ b[i]);
        return diff == 0;
    }
};

} // namespace common
