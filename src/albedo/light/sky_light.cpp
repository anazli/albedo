#include "albedo/light/sky_light.hpp"

#include "albedo/core/real.hpp"

namespace albedo {

Rgb SkyLight::emittedRadiance(Vector3r direction) const noexcept
{
    const Real height{0.5 * (direction.y + 1)};
    return nadir * (1 - height) + zenith * height;
}

} // namespace albedo
