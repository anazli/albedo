#pragma once

#include "albedo/core/vector3.hpp"

namespace albedo {

/// `w` mirrored about the unit normal `n`: the direction leaving at the same
/// angle on the other side of `n`, in the plane of `w` and `n`. Both `w` and
/// the result point away from the surface.
[[nodiscard]] constexpr Vector3r reflect(Vector3r w, Vector3r n) noexcept
{
    return n * (2 * dot(w, n)) - w;
}

} // namespace albedo
