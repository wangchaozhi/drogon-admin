#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace modules::user::dto::validation {

inline std::string normalizeEmail(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    value = value.substr(first, last - first + 1);
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

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
