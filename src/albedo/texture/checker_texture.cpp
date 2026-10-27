#include "albedo/texture/checker_texture.hpp"

#include <cassert>
#include <cmath>

namespace albedo {

Rgb CheckerTexture::evaluate(const HitRecord& hit) const noexcept
{
    assert(frequency > 0 && std::isfinite(frequency) &&
           "checker frequency must be positive and finite");
    // The parity of i + j is found in floating point: converting the square
    // indices to an integer type could overflow for large texture coordinates.
    // fmod is exact, and an infinite or NaN sum gives NaN, which counts as even.
    const Real square_sum{std::floor(hit.uv.x * frequency) + std::floor(hit.uv.y * frequency)};
    return std::abs(std::fmod(square_sum, Real{2})) == 1 ? odd : even;
}

} // namespace albedo
