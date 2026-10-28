#include "albedo/texture/perlin_texture.hpp"

#include "albedo/texture/perlin_noise.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace albedo {

Rgb PerlinTexture::evaluate(const HitRecord& hit) const noexcept
{
    assert(scale > 0 && std::isfinite(scale) && "perlin scale must be positive and finite");
    assert(veins >= 0 && std::isfinite(veins) && "perlin veins must be finite and non-negative");
    const Point3r scaled{hit.point.x * scale, hit.point.y * scale, hit.point.z * scale};
    const Real swirl{turbulence(scaled)};
    Real weight{std::min(swirl, Real{1})};
    if (veins > 0) {
        weight = (1 + std::sin(scaled.x + veins * swirl)) / 2;
        // sin of an infinite or NaN argument is NaN; blend evenly instead.
        if (std::isnan(weight)) {
            weight = Real{0.5};
        }
    }
    return low * (1 - weight) + high * weight;
}

} // namespace albedo
