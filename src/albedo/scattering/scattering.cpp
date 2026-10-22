#include "albedo/scattering/scattering.hpp"

namespace albedo {

Scattering::Scattering(Lobe lobe, Normal3r normal) noexcept
    : lobe{lobe}, basis{OrthonormalBasis::fromNormal(normal)}
{
}

// The free lobe functions are qualified because the members of the same name hide them.

Rgb Scattering::evaluate(Vector3r wo, Vector3r wi) const
{
    return albedo::evaluate(lobe, basis.toLocal(wo), basis.toLocal(wi));
}

std::optional<ScatterSample> Scattering::sample(Vector3r wo, Point2r u) const
{
    std::optional<ScatterSample> result{albedo::sample(lobe, basis.toLocal(wo), u)};
    if (result) {
        result->direction = basis.toWorld(result->direction);
    }
    return result;
}

Real Scattering::pdf(Vector3r wo, Vector3r wi) const
{
    return albedo::pdf(lobe, basis.toLocal(wo), basis.toLocal(wi));
}

} // namespace albedo
