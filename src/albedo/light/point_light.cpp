#include "albedo/light/point_light.hpp"

#include <cmath>

namespace albedo {

std::optional<LightSample> PointLight::sampleIncidentLight(Point3r point,
                                                           Point2r /*u*/) const noexcept
{
    const Vector3r to_light{position - point};
    const Real distance_squared{lengthSquared(to_light)};
    if (distance_squared == 0) {
        return std::nullopt;
    }
    const Real distance{std::sqrt(distance_squared)};
    return LightSample{.direction = to_light / distance,
                       .radiance = intensity / distance_squared,
                       .distance = distance,
                       .pdf = 1,
                       .delta = true};
}

} // namespace albedo
