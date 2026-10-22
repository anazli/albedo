#pragma once

#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"

#include <optional>

namespace albedo {

/// `w` mirrored about the unit normal `n`: the direction leaving at the same
/// angle on the other side of `n`, in the plane of `w` and `n`. Both `w` and
/// the result point away from the surface.
[[nodiscard]] constexpr Vector3r reflect(Vector3r w, Vector3r n) noexcept
{
    return n * (2 * dot(w, n)) - w;
}

/// `w` refracted through a smooth boundary with unit normal `n`, by Snell's law:
/// the direction on the far side of `n`, in the plane of `w` and `n`. `eta` is
/// the index on the far side divided by the index on `w`'s side. Returns nothing
/// under total internal reflection. Both `w` and the result point away from the
/// surface. Precondition: `w` is a unit vector with dot(w, n) > 0, and `eta` > 0.
[[nodiscard]] std::optional<Vector3r> refract(Vector3r w, Vector3r n, Real eta) noexcept;

} // namespace albedo
