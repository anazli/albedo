#include "albedo/light/sky_light.hpp"

#include "albedo/core/real.hpp"
#include "albedo/sampling/warps.hpp"

#include <limits>

namespace albedo {

Rgb SkyLight::emittedRadiance(Vector3r direction) const noexcept
{
    const Real height{0.5 * (direction.y + 1)};
    return nadir * (1 - height) + zenith * height;
}

std::optional<LightSample> SkyLight::sampleIncidentLight(Point3r /*point*/,
                                                         Point2r u) const noexcept
{
    const Vector3r direction{sampleUniformSphere(u)};
    return LightSample{.direction = direction,
                       .radiance = emittedRadiance(direction),
                       .distance = std::numeric_limits<Real>::infinity(),
                       .pdf = uniformSpherePdf()};
}

} // namespace albedo
