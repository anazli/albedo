#pragma once

#include "albedo/core/interval.hpp"
#include "albedo/core/normal3.hpp"
#include "albedo/core/ray.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"
#include "albedo/scene/hit_record.hpp"

#include <limits>

namespace albedo {

/// How far off the surface a ray leaving it starts, along the normal, so that
/// rounding error in the hit point cannot make it hit the same surface again at
/// t ~ 0. Far above that error for scenes of roughly unit to thousand-unit
/// scale, and far below any feature size that matters there.
inline constexpr Real surface_offset{1e-7};

/// Every hit ahead of a ray's origin, as a path's next intersection looks for.
inline constexpr Interval in_front_of_origin{.min = 0,
                                             .max = std::numeric_limits<Real>::infinity()};

/// The ray leaving `hit` in `direction`, started off the surface on the side the
/// direction leaves: in front for reflection, behind for refraction.
[[nodiscard]] inline Ray rayLeaving(const HitRecord& hit, Vector3r direction) noexcept
{
    const Real side{dot(direction, hit.normal) >= 0 ? Real{1} : Real{-1}};
    return {.origin = hit.point + asVector(hit.normal) * (side * surface_offset),
            .direction = direction};
}

} // namespace albedo
