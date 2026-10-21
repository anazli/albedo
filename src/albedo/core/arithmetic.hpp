#pragma once

#include <concepts>
#include <type_traits>

namespace albedo {

/// Scalar types usable as components of the math types: cv-unqualified
/// floating-point and signed integer types.
///
/// Unsigned types are excluded because negation and point differences wrap
/// around; `bool` and character types only behave like numbers by accident.
template <typename T>
concept Arithmetic =
    std::same_as<T, std::remove_cv_t<T>> &&
    (std::floating_point<T> ||
     (std::signed_integral<T> && !std::same_as<T, char> && !std::same_as<T, wchar_t>));

namespace detail {

/// True when `Factor` is a floating-point type whose conversion to the component
/// type `T` narrows: to an integer, or to a floating type that cannot represent
/// every `Factor` value. Uses the language's own narrowing rule (brace
/// initialization from a non-constant), so the answer is the same on every
/// platform, e.g. `long double` -> `double` counts even where both are 8 bytes.
/// Integer factors are deliberately not checked, so `v * 2` keeps working.
template <typename Factor, typename T>
concept LossyFactor = std::floating_point<Factor> && !requires(Factor f) { T{f}; };

} // namespace detail

} // namespace albedo
