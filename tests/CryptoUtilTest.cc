#include "common/CryptoUtil.h"
#include "common/JwtUtil.h"
#include "modules/user/dto/Validation.h"

#include <cassert>
#include <iostream>

namespace {

std::string makeToken(Json::Value header,
                      Json::Value payload,
                      const std::string& secret) {
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    const auto headerB64 = common::CryptoUtil::base64UrlEncode(
        Json::writeString(writer, header));
    const auto payloadB64 = common::CryptoUtil::base64UrlEncode(
        Json::writeString(writer, payload));
    const auto input = headerB64 + "." + payloadB64;
    return input + "." + common::CryptoUtil::base64UrlEncode(
        common::CryptoUtil::hmacSha256Raw(secret, input));
}

} // namespace

int main() {
    using common::CryptoUtil;

    constexpr auto password = "Correct-Horse-Battery-Staple!";
    const auto first = CryptoUtil::hashPassword(password);
    const auto second = CryptoUtil::hashPassword(password);

    assert(first.rfind("$argon2id$", 0) == 0);
    assert(first != second);
    assert(CryptoUtil::verifyPassword(password, first));
    assert(!CryptoUtil::verifyPassword("wrong password", first));
    assert(!CryptoUtil::passwordNeedsRehash(first));

    // Legacy sha256("drogon_admin_salt_v1" || "legacy-password").
    constexpr auto legacy =
        "fa064fd274d1d5b3a2b204b249f1d132339ad2317e1a38096e8e8f3923c31e48";
    assert(CryptoUtil::verifyPassword("legacy-password", legacy));
    assert(CryptoUtil::passwordNeedsRehash(legacy));

    using namespace modules::user::dto::validation;
    assert(validEmail("admin@example.com"));
    assert(!validEmail("invalid@example"));
    assert(!validEmail("two@@example.com"));
    assert(validPassword("Correct-Horse-2026!"));
    assert(!validPassword("too-short"));

    constexpr auto jwtSecret =
        "test-only-secret-with-at-least-32-characters";
    const auto token = common::JwtUtil::sign(
        42, "admin@example.com", 7, jwtSecret, 300);
    std::string jwtError;
    const auto claims = common::JwtUtil::verify(token, jwtSecret, jwtError);
    assert(claims);
    assert(claims->userId == 42);
    assert(claims->tokenVersion == 7);
    assert(claims->email == "admin@example.com");

    auto tampered = token;
    tampered.back() = tampered.back() == 'A' ? 'B' : 'A';
    assert(!common::JwtUtil::verify(tampered, jwtSecret, jwtError));

    Json::Value header;
    header["alg"] = "HS256";
    header["typ"] = "JWT";
    Json::Value legacyClaims;
    legacyClaims["sub"] = 42;
    legacyClaims["email"] = "admin@example.com";
    legacyClaims["iat"] = static_cast<Json::Int64>(
        common::TimeUtil::nowSec());
    legacyClaims["exp"] = static_cast<Json::Int64>(
        common::TimeUtil::nowSec() + 300);
    const auto legacyToken = makeToken(header, legacyClaims, jwtSecret);
    assert(!common::JwtUtil::verify(legacyToken, jwtSecret, jwtError));
    assert(jwtError == "invalid token claims");

    std::cout << "CryptoUtil and JwtUtil tests passed\n";
    return 0;
}
