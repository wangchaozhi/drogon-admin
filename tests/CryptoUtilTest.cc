#include "common/CryptoUtil.h"
#include "modules/user/dto/Validation.h"

#include <cassert>
#include <iostream>

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

    std::cout << "CryptoUtil tests passed\n";
    return 0;
}
