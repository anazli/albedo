#pragma once

#include "albedo/core/arithmetic.hpp"
#include "albedo/core/real.hpp"

namespace albedo {

/// A position in 2D: pixel coordinates, sample positions, texture coordinates.
/// Operations are added when a step needs them.
template <Arithmetic T>
struct Point2 {
    T x{};
    T y{};

    [[nodiscard]] constexpr bool operator==(const Point2&) const noexcept = default;
};

/// 2D point with `Real` components, e.g. sample or texture coordinates.
using Point2r = Point2<Real>;
/// 2D point with integer components, e.g. pixel coordinates.
using Point2i = Point2<int>;

} // namespace albedo
