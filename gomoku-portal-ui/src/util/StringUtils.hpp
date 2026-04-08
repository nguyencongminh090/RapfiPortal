/*
 *  Portal Gomoku UI — String Utilities
 *  Lightweight string helpers. No external dependencies.
 */

#pragma once

#include <algorithm>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace util {

/// Trim whitespace from both ends of a string view.
[[nodiscard]] inline std::string_view trim(std::string_view sv) {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front())))
        sv.remove_prefix(1);
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back())))
        sv.remove_suffix(1);
    return sv;
}

/// Split a string by a delimiter character.
[[nodiscard]] inline std::vector<std::string_view> split(std::string_view sv, char delim) {
    std::vector<std::string_view> parts;
    while (true) {
        auto pos = sv.find(delim);
        if (pos == std::string_view::npos) {
            if (!sv.empty()) parts.push_back(sv);
            break;
        }
        parts.push_back(sv.substr(0, pos));
        sv.remove_prefix(pos + 1);
    }
    return parts;
}

/// Parse an integer from a string view. Returns nullopt on failure.
[[nodiscard]] inline std::optional<int> parseInt(std::string_view sv) {
    sv = trim(sv);
    int value = 0;
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
    if (ec == std::errc{} && ptr == sv.data() + sv.size())
        return value;
    return std::nullopt;
}

/// Convert a string to uppercase in-place.
inline void toUpper(std::string& s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::toupper(c); });
}

/// Check if a string starts with a given prefix.
[[nodiscard]] inline bool startsWith(std::string_view sv, std::string_view prefix) {
    return sv.size() >= prefix.size() && sv.substr(0, prefix.size()) == prefix;
}

}  // namespace util
