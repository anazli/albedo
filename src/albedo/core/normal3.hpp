#pragma once

#include "albedo/core/arithmetic.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"

#include <concepts>
#include <type_traits>

namespace albedo {

/// A surface normal.
///
/// Kept distinct from `Vector3` because normals transform with the inverse
/// transpose, so mixing them in arithmetic by accident is a bug. Convert
/// explicitly with `asVector` / `asNormal`.
template <std::floating_point T>
struct Normal3 {
    T x{};
    T y{};
    T z{};

    [[nodiscard]] constexpr bool operator==(const Normal3&) const noexcept = default;
};

/// Normal with `Real` components.
using Normal3r = Normal3<Real>;

/// The same components viewed as a vector.
template <std::floating_point T>
[[nodiscard]] constexpr Vector3<T> asVector(Normal3<T> n) noexcept
{
    return {n.x, n.y, n.z};
}

/// The same components viewed as a normal.
template <std::floating_point T>
[[nodiscard]] constexpr Normal3<T> asNormal(Vector3<T> v) noexcept
{
    return {v.x, v.y, v.z};
}

/// The normal facing the opposite way.
template <std::floating_point T>
[[nodiscard]] constexpr Normal3<T> operator-(Normal3<T> n) noexcept
{
    return asNormal(-asVector(n));
}

/// Scales every component by `scale`.
template <std::floating_point T>
[[nodiscard]] constexpr Normal3<T> operator*(Normal3<T> n, std::type_identity_t<T> scale) noexcept
{
    return asNormal(asVector(n) * scale);
}

/// Scales every component by `scale`.
template <std::floating_point T>
[[nodiscard]] constexpr Normal3<T> operator*(std::type_identity_t<T> scale, Normal3<T> n) noexcept
{
    return n * scale;
}

/// Scaling by a factor that would be narrowed is rejected.
template <std::floating_point T, typename Factor>
    requires detail::LossyFactor<Factor, T>
Normal3<T> operator*(Normal3<T>, Factor) = delete;

/// Scaling by a factor that would be narrowed is rejected.
template <std::floating_point T, typename Factor>
    requires detail::LossyFactor<Factor, T>
Normal3<T> operator*(Factor, Normal3<T>) = delete;

/// Dot product of a normal and a vector.
template <std::floating_point T>
[[nodiscard]] constexpr T dot(Normal3<T> n, Vector3<T> v) noexcept
{
    return dot(asVector(n), v);
}

/// Dot product of a vector and a normal.
template <std::floating_point T>
[[nodiscard]] constexpr T dot(Vector3<T> v, Normal3<T> n) noexcept
{
    return dot(v, asVector(n));
}

/// Dot product of two normals.
template <std::floating_point T>
[[nodiscard]] constexpr T dot(Normal3<T> a, Normal3<T> b) noexcept
{
    return dot(asVector(a), asVector(b));
}

/// Unit normal with the same orientation.
/// Precondition: as for `normalized(Vector3<T>)`; the length must be positive and
/// within the range documented there.
template <std::floating_point T>
[[nodiscard]] Normal3<T> normalized(Normal3<T> n) noexcept
{
    return asNormal(normalized(asVector(n)));
}

} // namespace albedo
