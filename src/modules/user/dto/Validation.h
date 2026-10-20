#pragma once

#include <cctype>
#include <string_view>

namespace modules::user::dto::validation {

inline bool validEmail(std::string_view value) {
    if (value.empty() || value.size() > 254) return false;
    if (value.front() == '.' || value.back() == '.') return false;
    const auto at = value.find('@');
    if (at == std::string_view::npos || at == 0 || at + 1 >= value.size())
        return false;
    if (value.find('@', at + 1) != std::string_view::npos) return false;
    const auto dot = value.find('.', at + 2);
    if (dot == std::string_view::npos || dot + 1 >= value.size()) return false;
    for (unsigned char ch : value) {
        if (std::isspace(ch) || std::iscntrl(ch)) return false;
    }
    return true;
}

inline bool validPassword(std::string_view value) {
    return value.size() >= 12 && value.size() <= 256;
}

} // namespace modules::user::dto::validation
