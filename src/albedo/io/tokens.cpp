#include "albedo/io/tokens.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <system_error>

namespace albedo {

std::optional<std::string_view> Tokens::next() noexcept
{
    constexpr std::string_view whitespace{" \t\r\v\f"};
    const std::size_t start{rest.find_first_not_of(whitespace)};
    if (start == std::string_view::npos) {
        rest = {};
        return std::nullopt;
    }
    rest.remove_prefix(start);
    const std::size_t end{std::min(rest.find_first_of(whitespace), rest.size())};
    const std::string_view token{rest.substr(0, end)};
    rest.remove_prefix(end);
    return token;
}

std::optional<Real> parseReal(std::string_view token) noexcept
{
    // std::from_chars rejects a leading '+' itself, so it is skipped here, but
    // only once and never before another sign.
    if (token.starts_with('+')) {
        token.remove_prefix(1);
        if (token.starts_with('+') || token.starts_with('-')) {
            return std::nullopt;
        }
    }
    Real value{};
    const auto [end, error]{std::from_chars(token.data(), token.data() + token.size(), value)};
    if (error != std::errc{} || end != token.data() + token.size() || !std::isfinite(value)) {
        return std::nullopt;
    }
    return value;
}

std::optional<long long> parseInteger(std::string_view token) noexcept
{
    long long value{};
    const auto [end, error]{std::from_chars(token.data(), token.data() + token.size(), value)};
    if (error != std::errc{} || end != token.data() + token.size() || token.empty()) {
        return std::nullopt;
    }
    return value;
}

} // namespace albedo
