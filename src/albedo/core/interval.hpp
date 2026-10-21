#pragma once

#include "albedo/core/real.hpp"

#include <algorithm>
#include <cassert>
#include <limits>

namespace albedo {

/// A closed range of real numbers `[min, max]`, e.g. the valid `t` range of a ray.
///
/// Default-constructed intervals are empty (`min = +inf`, `max = -inf`), which
/// makes them the identity for growing an interval to enclose values.
struct Interval {
    Real min{std::numeric_limits<Real>::infinity()};
    Real max{-std::numeric_limits<Real>::infinity()};

    /// True when no value lies inside. A single point (`min == max`) is not empty.
    [[nodiscard]] constexpr bool isEmpty() const noexcept { return min > max; }

    /// `max - min`; negative for an empty interval.
    [[nodiscard]] constexpr Real size() const noexcept { return max - min; }

    /// True when `min <= x <= max`.
    [[nodiscard]] constexpr bool contains(Real x) const noexcept { return min <= x && x <= max; }

    /// True when `min < x < max`. Hit tests use this to reject hits at the bounds.
    [[nodiscard]] constexpr bool surrounds(Real x) const noexcept { return min < x && x < max; }

    /// `x` limited to the interval. Precondition: the interval is not empty.
    [[nodiscard]] constexpr Real clamp(Real x) const noexcept
    {
        assert(!isEmpty() && "cannot clamp to an empty interval");
        return std::clamp(x, min, max);
    }
};

/// The interval containing no values.
inline constexpr Interval empty_interval{};

/// The interval containing every value, including the infinities.
inline constexpr Interval universe_interval{.min = -std::numeric_limits<Real>::infinity(),
                                            .max = std::numeric_limits<Real>::infinity()};

} // namespace albedo
