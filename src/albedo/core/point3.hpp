#pragma once

#include "albedo/core/arithmetic.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"

#include <algorithm>
#include <concepts>

namespace albedo {

/// A position in 3D space.
///
/// Only affine operations are defined: point − point gives a vector and
/// point ± vector gives a point. Adding or scaling points does not compile.
template <Arithmetic T>
struct Point3 {
    T x{};
    T y{};
    T z{};

    [[nodiscard]] constexpr bool operator==(const Point3&) const noexcept = default;

    /// Moves the point by `offset`.
    constexpr Point3& operator+=(Vector3<T> offset) noexcept
    {
        x += offset.x;
        y += offset.y;
        z += offset.z;
        return *this;
    }

    /// Moves the point by `-offset`.
    constexpr Point3& operator-=(Vector3<T> offset) noexcept
    {
        x -= offset.x;
        y -= offset.y;
        z -= offset.z;
        return *this;
    }
};

/// Point with `Real` components, the default for geometry.
using Point3r = Point3<Real>;
/// Point with integer components.
using Point3i = Point3<int>;

/// The vector from `b` to `a`.
template <Arithmetic T>
[[nodiscard]] constexpr Vector3<T> operator-(Point3<T> a, Point3<T> b) noexcept
{
    return Vector3<T>{a.x, a.y, a.z} - Vector3<T>{b.x, b.y, b.z};
}

/// The point `offset` away from `p`.
template <Arithmetic T>
[[nodiscard]] constexpr Point3<T> operator+(Point3<T> p, Vector3<T> offset) noexcept
{
    return p += offset;
}

/// The point `-offset` away from `p`.
template <Arithmetic T>
[[nodiscard]] constexpr Point3<T> operator-(Point3<T> p, Vector3<T> offset) noexcept
{
    return p -= offset;
}

/// The point taking the smaller coordinate of `a` and `b` on each axis.
template <Arithmetic T>
[[nodiscard]] constexpr Point3<T> componentMin(Point3<T> a, Point3<T> b) noexcept
{
    return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
}

/// The point taking the larger coordinate of `a` and `b` on each axis.
template <Arithmetic T>
[[nodiscard]] constexpr Point3<T> componentMax(Point3<T> a, Point3<T> b) noexcept
{
    return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
}

/// Squared distance between two points.
template <Arithmetic T>
[[nodiscard]] constexpr T distanceSquared(Point3<T> a, Point3<T> b) noexcept
{
    return lengthSquared(a - b);
}

/// Distance between two points.
template <std::floating_point T>
[[nodiscard]] T distance(Point3<T> a, Point3<T> b) noexcept
{
    return length(a - b);
}

} // namespace albedo
