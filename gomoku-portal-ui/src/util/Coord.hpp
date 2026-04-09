/*
 *  Portal Gomoku UI — Coordinate Utilities
 *  Board coordinate ↔ protocol coordinate conversion.
 */

#pragma once

#include <utility>
#include <optional>
#include <string_view>
#include <cctype>
#include <charconv>

namespace util {

/// Board coordinate representation.
/// (0,0) = top-left corner. x = column, y = row.
struct Coord {
    int x = -1;
    int y = -1;

    [[nodiscard]] bool isValid(int boardSize) const {
        return x >= 0 && x < boardSize && y >= 0 && y < boardSize;
    }

    bool operator==(const Coord& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Coord& o) const { return !(*this == o); }
    bool operator<(const Coord& o) const {
        return (y != o.y) ? y < o.y : x < o.x;
    }

    /// Convert board coord to 1D index (row-major).
    [[nodiscard]] int toIndex(int boardSize) const { return y * boardSize + x; }

    /// Create from 1D index (row-major).
    [[nodiscard]] static Coord fromIndex(int idx, int boardSize) {
        return {idx % boardSize, idx / boardSize};
    }

    /// Invalid sentinel coord.
    [[nodiscard]] static constexpr Coord none() { return {-1, -1}; }

    /// Parse a coordinate from a string (handles both "x,y" and alphanumeric "H8").
    /// Strips leading/trailing whitespace, parentheses, and semicolons.
    [[nodiscard]] static std::optional<Coord> parse(std::string_view token) {
        // Strip common wrappers: (), ;, whitespace
        while (!token.empty() && (token.front() == '(' || std::isspace(static_cast<unsigned char>(token.front()))))
            token.remove_prefix(1);
        while (!token.empty() && (token.back() == ')' || token.back() == ';' || std::isspace(static_cast<unsigned char>(token.back()))))
            token.remove_suffix(1);

        if (token.empty()) return std::nullopt;

        // Pattern 1: Alphanumeric "H8"
        if (std::isalpha(static_cast<unsigned char>(token[0]))) {
            int x = std::toupper(static_cast<unsigned char>(token[0])) - 'A';
            int yValue = 0;
            bool hasDigit = false;
            for (size_t i = 1; i < token.size(); ++i) {
                if (std::isdigit(static_cast<unsigned char>(token[i]))) {
                    yValue = yValue * 10 + (token[i] - '0');
                    hasDigit = true;
                } else {
                    return std::nullopt;
                }
            }
            if (hasDigit) return Coord{x, yValue - 1}; // 1-indexed to 0-indexed
            return std::nullopt;
        }

        // Pattern 2: Numeric "7,7"
        auto comma = token.find(',');
        if (comma != std::string_view::npos) {
            int x = -1, y = -1;
            auto xPart = token.substr(0, comma);
            auto yPart = token.substr(comma + 1);
            // Handle optional third part (color) by finding second comma
            auto secondComma = yPart.find(',');
            if (secondComma != std::string_view::npos) {
                yPart = yPart.substr(0, secondComma);
            }

            if (auto [ptr, ec] = std::from_chars(xPart.data(), xPart.data() + xPart.size(), x); ec != std::errc{})
                return std::nullopt;
            if (auto [ptr, ec] = std::from_chars(yPart.data(), yPart.data() + yPart.size(), y); ec != std::errc{})
                return std::nullopt;

            return Coord{x, y};
        }

        return std::nullopt;
    }
};

/// Portal pair: two coordinates linked as a bidirectional teleporter.
struct PortalPair {
    Coord a;
    Coord b;
};

}  // namespace util
