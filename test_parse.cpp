#include <iostream>
#include <string_view>
#include <optional>
#include <charconv>

struct Coord {
    int x = -1;
    int y = -1;

    static std::optional<Coord> parse(std::string_view token) {
        while (!token.empty() && (token.front() == '(' || std::isspace(static_cast<unsigned char>(token.front()))))
            token.remove_prefix(1);
        while (!token.empty() && (token.back() == ')' || token.back() == ';' || std::isspace(static_cast<unsigned char>(token.back()))))
            token.remove_suffix(1);

        if (token.empty()) return std::nullopt;

        if (std::isalpha(static_cast<unsigned char>(token[0]))) {
            int x = std::toupper(static_cast<unsigned char>(token[0])) - 'A';
            int yValue = 0;
            auto sub = token.substr(1);
            if (auto [ptr, ec] = std::from_chars(sub.data(), sub.data() + sub.size(), yValue); ec == std::errc{}) {
                return Coord{x, yValue - 1};
            }
            return std::nullopt;
        }
        return std::nullopt;
    }
};

int main() {
    auto c = Coord::parse("H9");
    if (c) {
        std::cout << "Success: " << c->x << "," << c->y << "\n";
    } else {
        std::cout << "Failure!\n";
    }
    return 0;
}
