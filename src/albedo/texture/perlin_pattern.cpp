#include "albedo/texture/perlin_pattern.hpp"

#include "albedo/texture/perlin_noise.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace albedo {

Real PerlinPattern::weightAt(Point3r point) const noexcept
{
    assert(scale > 0 && std::isfinite(scale) && "perlin scale must be positive and finite");
    assert(veins >= 0 && std::isfinite(veins) && "perlin veins must be finite and non-negative");
    const Point3r scaled{point.x * scale, point.y * scale, point.z * scale};
    const Real swirl{turbulence(scaled)};
    if (veins == 0) {
        return std::min(swirl, Real{1});
    }
    const Real weight{(1 + std::sin(scaled.x + veins * swirl)) / 2};
    // sin of an infinite or NaN argument is NaN; weigh evenly instead.
    return std::isnan(weight) ? Real{0.5} : weight;
}

} // namespace albedo
