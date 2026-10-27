#pragma once

#include "albedo/core/real.hpp"

#include <optional>
#include <string_view>

namespace albedo {

/// The words of a line of text, split at runs of spaces, tabs, carriage returns,
/// vertical tabs and form feeds. Every other character, including `#`, belongs
/// to a word; callers strip comments first.
class Tokens {
public:
    /// Splits `line`, which must outlive the tokens.
    explicit Tokens(std::string_view line) noexcept : rest{line} {}

    /// The next word, or nothing once the line is used up.
    [[nodiscard]] std::optional<std::string_view> next() noexcept;

private:
    std::string_view rest;
};

/// `token` as a finite number, or nothing unless the whole token is one decimal
/// floating-point number (an optional sign, digits with an optional point, and an
/// optional exponent). A leading `+` is accepted; infinities, NaN, hexadecimal
/// forms and values too large for `Real` are not. Subnormal values are accepted;
/// values too small even for those (such as 1e-400) are rejected rather than read
/// as zero, wherever the standard library reports them out of range, as
/// libstdc++ does.
[[nodiscard]] std::optional<Real> parseReal(std::string_view token) noexcept;

/// `token` as a signed decimal integer, or nothing unless the whole token is one
/// that fits a `long long`. A leading `-` is accepted, a leading `+` is not.
[[nodiscard]] std::optional<long long> parseInteger(std::string_view token) noexcept;

} // namespace albedo
