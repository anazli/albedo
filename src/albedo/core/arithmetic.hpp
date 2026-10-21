#pragma once

#include <concepts>

namespace albedo {

/// Scalar types usable as components of the math types: integers and
/// floating point, excluding `bool`, which only behaves like a number by accident.
template <typename T>
concept Arithmetic = (std::integral<T> || std::floating_point<T>) && !std::same_as<T, bool>;

} // namespace albedo
