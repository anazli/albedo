#pragma once

#include "albedo/core/arithmetic.hpp"
#include "albedo/core/point3.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"

#include <cassert>
#include <limits>

namespace albedo {

/// An axis-aligned box `[min, max]` on every axis, inclusive of its faces.
///
/// Default-constructed boxes are empty (`min` at the largest value, `max` at the
/// lowest), which makes them the identity for `merged`.
template <Arithmetic T>
struct Bounds3 {
    Point3<T> min{std::numeric_limits<T>::max(), std::numeric_limits<T>::max(),
                  std::numeric_limits<T>::max()};
    Point3<T> max{std::numeric_limits<T>::lowest(), std::numeric_limits<T>::lowest(),
                  std::numeric_limits<T>::lowest()};

    [[nodiscard]] constexpr bool operator==(const Bounds3&) const noexcept = default;

    /// True when `min > max` on any axis. A single point is not empty.
    [[nodiscard]] constexpr bool isEmpty() const noexcept
    {
        return min.x > max.x || min.y > max.y || min.z > max.z;
    }

    /// True when `p` lies inside or on a face.
    [[nodiscard]] constexpr bool contains(Point3<T> p) const noexcept
    {
        return min.x <= p.x && p.x <= max.x && min.y <= p.y && p.y <= max.y && min.z <= p.z &&
               p.z <= max.z;
    }

    /// The diagonal from `min` to `max`. Precondition: the box is not empty.
    [[nodiscard]] constexpr Vector3<T> extent() const noexcept
    {
        assert(!isEmpty() && "an empty box has no extent");
        return max - min;
    }

    /// Total area of the six faces; zero for an empty box. Used by the SAH BVH build.
    [[nodiscard]] constexpr T surfaceArea() const noexcept
    {
        if (isEmpty()) {
            return T{0};
        }
        const Vector3<T> d{extent()};
        return 2 * (d.x * d.y + d.y * d.z + d.z * d.x);
    }
};

/// Box with `Real` coordinates, the default for geometry.
using Bounds3r = Bounds3<Real>;
/// Box with integer coordinates.
using Bounds3i = Bounds3<int>;

/// The smallest box enclosing `box` and `p`.
template <Arithmetic T>
[[nodiscard]] constexpr Bounds3<T> merged(const Bounds3<T>& box, Point3<T> p) noexcept
{
    return {.min = componentMin(box.min, p), .max = componentMax(box.max, p)};
}

/// The smallest box enclosing both `a` and `b`.
template <Arithmetic T>
[[nodiscard]] constexpr Bounds3<T> merged(const Bounds3<T>& a, const Bounds3<T>& b) noexcept
{
    return {.min = componentMin(a.min, b.min), .max = componentMax(a.max, b.max)};
}

} // namespace albedo
