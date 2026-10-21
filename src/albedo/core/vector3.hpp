#pragma once

#include "albedo/core/arithmetic.hpp"
#include "albedo/core/real.hpp"

#include <cassert>
#include <cmath>
#include <concepts>
#include <type_traits>

namespace albedo {

/// A direction or displacement in 3D space.
///
/// Binary operators are built on the compound ones so that small integer
/// types convert back after promotion instead of failing on narrowing.
/// Scalar parameters use `std::type_identity_t` so `v * 2` works for a
/// `Vector3r` without deducing conflicting types.
template <Arithmetic T>
struct Vector3 {
    T x{};
    T y{};
    T z{};

    [[nodiscard]] constexpr bool operator==(const Vector3&) const noexcept = default;

    constexpr Vector3& operator+=(Vector3 other) noexcept
    {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    constexpr Vector3& operator-=(Vector3 other) noexcept
    {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    constexpr Vector3& operator*=(std::type_identity_t<T> scale) noexcept
    {
        x *= scale;
        y *= scale;
        z *= scale;
        return *this;
    }

    /// Integer vectors have no division: it would silently truncate.
    constexpr Vector3& operator/=(std::type_identity_t<T> divisor) noexcept
        requires std::floating_point<T>
    {
        return *this *= T{1} / divisor;
    }
};

/// Vector with `Real` components, the default for geometry.
using Vector3r = Vector3<Real>;
/// Vector with integer components.
using Vector3i = Vector3<int>;

/// Component-wise sum.
template <Arithmetic T>
[[nodiscard]] constexpr Vector3<T> operator+(Vector3<T> a, Vector3<T> b) noexcept
{
    return a += b;
}

/// Component-wise difference.
template <Arithmetic T>
[[nodiscard]] constexpr Vector3<T> operator-(Vector3<T> a, Vector3<T> b) noexcept
{
    return a -= b;
}

/// The vector pointing the opposite way.
template <Arithmetic T>
[[nodiscard]] constexpr Vector3<T> operator-(Vector3<T> v) noexcept
{
    return Vector3<T>{} - v;
}

/// Scales every component by `scale`.
template <Arithmetic T>
[[nodiscard]] constexpr Vector3<T> operator*(Vector3<T> v, std::type_identity_t<T> scale) noexcept
{
    return v *= scale;
}

/// Scales every component by `scale`.
template <Arithmetic T>
[[nodiscard]] constexpr Vector3<T> operator*(std::type_identity_t<T> scale, Vector3<T> v) noexcept
{
    return v *= scale;
}

/// Divides every component by `divisor`. Floating point only.
template <std::floating_point T>
[[nodiscard]] constexpr Vector3<T> operator/(Vector3<T> v, std::type_identity_t<T> divisor) noexcept
{
    return v /= divisor;
}

/// Dot product.
template <Arithmetic T>
[[nodiscard]] constexpr T dot(Vector3<T> a, Vector3<T> b) noexcept
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/// Cross product, following the right-hand rule.
template <Arithmetic T>
[[nodiscard]] constexpr Vector3<T> cross(Vector3<T> a, Vector3<T> b) noexcept
{
    return {static_cast<T>(a.y * b.z - a.z * b.y), static_cast<T>(a.z * b.x - a.x * b.z),
            static_cast<T>(a.x * b.y - a.y * b.x)};
}

/// Squared Euclidean length; cheaper than `length` when only comparing.
template <Arithmetic T>
[[nodiscard]] constexpr T lengthSquared(Vector3<T> v) noexcept
{
    return dot(v, v);
}

/// Euclidean length.
template <std::floating_point T>
[[nodiscard]] T length(Vector3<T> v) noexcept
{
    return std::sqrt(lengthSquared(v));
}

/// Unit vector with the same direction.
/// Precondition: `length(v) > 0`.
template <std::floating_point T>
[[nodiscard]] Vector3<T> normalized(Vector3<T> v) noexcept
{
    const T len{length(v)};
    assert(len > T{0} && "cannot normalize a zero-length vector");
    return v / len;
}

} // namespace albedo
