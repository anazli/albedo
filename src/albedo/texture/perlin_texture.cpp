#include "albedo/texture/perlin_texture.hpp"

#include "albedo/texture/perlin_pattern.hpp"

namespace albedo {

Rgb PerlinTexture::evaluate(const HitRecord& hit) const noexcept
{
    const Real weight{PerlinPattern{.scale = scale, .veins = veins}.weightAt(hit.point)};
    return low * (1 - weight) + high * weight;
}

} // namespace albedo
