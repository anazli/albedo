#include "albedo/scattering/scattering.hpp"

namespace albedo {

Scattering::Scattering(Lobe lobe, Normal3r normal) noexcept
    : Scattering{lobe, {.shading = normal, .geometric = normal}}
{
}

Scattering::Scattering(Lobe lobe, SurfaceNormals normals) noexcept
    : lobe{lobe}, basis{OrthonormalBasis::fromNormal(normals.shading)}, geometric{normals.geometric}
{
}

// The free lobe functions are qualified because the members of the same name hide them.

Rgb Scattering::evaluate(Vector3r wo, Vector3r wi) const
{
    if (crossesSurface(wo) || crossesSurface(wi)) {
        return {};
    }
    return albedo::evaluate(lobe, basis.toLocal(wo), basis.toLocal(wi));
}

std::optional<ScatterSample> Scattering::sample(Vector3r wo, Real u_choice, Point2r u) const
{
    if (crossesSurface(wo)) {
        return std::nullopt;
    }
    std::optional<ScatterSample> result{albedo::sample(lobe, basis.toLocal(wo), u_choice, u)};
    if (!result) {
        return result;
    }
    result->direction = basis.toWorld(result->direction);
    if (crossesSurface(result->direction)) {
        return std::nullopt;
    }
    return result;
}

Real Scattering::pdf(Vector3r wo, Vector3r wi) const
{
    if (crossesSurface(wo) || crossesSurface(wi)) {
        return 0;
    }
    return albedo::pdf(lobe, basis.toLocal(wo), basis.toLocal(wi));
}

bool Scattering::crossesSurface(Vector3r direction) const noexcept
{
    // Compared by sign rather than multiplied, so that with equal normals both
    // sides of the test are the same expression and can never disagree.
    return (dot(direction, basis.normal()) < 0) != (dot(direction, geometric) < 0);
}

} // namespace albedo
