#include "albedo/material/mix_material.hpp"

#include <cassert>

namespace albedo {

Scattering MixMaterial::scatteringAt(const HitRecord& hit) const
{
    assert(threshold >= 0 && threshold <= 1 && "mix threshold must be in [0, 1]");
    return albedo::scatteringAt(pattern.weightAt(hit.point) < threshold ? first : second, hit);
}

} // namespace albedo
