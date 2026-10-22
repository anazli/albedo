#include "albedo/scattering/ggx_distribution.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

namespace albedo {

namespace {

// Loose enough for vectors normalized in float precision or built from sums.
constexpr Real unit_length_tolerance{1e-6};

bool isUnit(Vector3r v) noexcept
{
    return std::abs(length(v) - 1) < unit_length_tolerance;
}

} // namespace

GgxDistribution::GgxDistribution(Real alpha) noexcept : width{alpha}
{
    assert(alpha > 0 && std::isfinite(alpha) && "GGX alpha must be positive and finite");
}

Real GgxDistribution::density(Vector3r m) const noexcept
{
    assert(isUnit(m) && "microfacet normal must have unit length");
    if (m.z <= 0) {
        return 0;
    }
    // Walter et al., "Microfacet Models for Refraction through Rough Surfaces"
    // (EGSR 2007), eq. 33, rewritten with cos^2 instead of tan^2 so it stays
    // finite at the normal: alpha^2 / (pi (cos^2 (alpha^2 - 1) + 1)^2).
    const Real alpha_squared{width * width};
    const Real denominator{m.z * m.z * (alpha_squared - 1) + 1};
    return alpha_squared / (pi * denominator * denominator);
}

Real GgxDistribution::masking(Vector3r w) const noexcept
{
    return 1 / (1 + lambda(w));
}

Real GgxDistribution::maskingShadowing(Vector3r wo, Vector3r wi) const noexcept
{
    // Heitz, "Understanding the Masking-Shadowing Function in Microfacet-Based
    // BRDFs" (JCGT 3(2), 2014), eq. 99: the height-correlated form, which
    // unlike the separable product accounts for masking and shadowing both
    // being more likely at low microsurface heights.
    return 1 / (1 + lambda(wo) + lambda(wi));
}

Vector3r GgxDistribution::sampleVisibleNormal(Vector3r wo, Point2r u) const noexcept
{
    assert(wo.z > 0 && "visible normals are sampled from above the surface");
    // Heitz, "Sampling the GGX Distribution of Visible Normals" (JCGT 7(4),
    // 2018). Stretching by alpha turns the distribution into that of a
    // hemisphere of radius 1, whose visible part seen from the stretched view
    // direction is a disk with half of it foreshortened.
    const Vector3r view{normalized(Vector3r{width * wo.x, width * wo.y, wo.z})};

    // An orthonormal frame around the view direction; any frame will do when
    // looking straight down.
    const Real length_squared{view.x * view.x + view.y * view.y};
    const Vector3r first_axis{length_squared > 0
                                  ? Vector3r{-view.y, view.x, 0} / std::sqrt(length_squared)
                                  : Vector3r{1, 0, 0}};
    const Vector3r second_axis{cross(view, first_axis)};

    // A uniform point on the unit disk, its far half squashed to the projected
    // area of the visible hemisphere.
    const Real radius{std::sqrt(u.x)};
    const Real phi{2 * pi * u.y};
    const Real t1{radius * std::cos(phi)};
    const Real blend{(1 + view.z) / 2};
    const Real t2{(1 - blend) * std::sqrt(std::max(Real{0}, 1 - t1 * t1)) +
                  blend * radius * std::sin(phi)};

    // Lift the point onto the hemisphere, then undo the stretch.
    const Real lift{std::sqrt(std::max(Real{0}, 1 - t1 * t1 - t2 * t2))};
    const Vector3r hemisphere_normal{first_axis * t1 + second_axis * t2 + view * lift};
    return normalized(Vector3r{width * hemisphere_normal.x, width * hemisphere_normal.y,
                               std::max(Real{0}, hemisphere_normal.z)});
}

Real GgxDistribution::visibleNormalPdf(Vector3r wo, Vector3r m) const noexcept
{
    // Heitz 2014, eq. 38 (the normalized distribution of visible normals).
    const Real facing{dot(wo, m)};
    if (wo.z <= 0 || facing <= 0) {
        return 0;
    }
    return masking(wo) * facing * density(m) / wo.z;
}

Real GgxDistribution::lambda(Vector3r w) const noexcept
{
    // Heitz 2014, eq. 72: Lambda = (sqrt(1 + alpha^2 tan^2(theta)) - 1) / 2.
    // tan^2 comes from the components directly, which stays accurate near the
    // normal where 1 - cos^2 would cancel.
    assert(isUnit(w) && "direction must have unit length");
    const Real cos_squared{w.z * w.z};
    if (cos_squared == 0) {
        return std::numeric_limits<Real>::infinity();
    }
    const Real tan_squared{(w.x * w.x + w.y * w.y) / cos_squared};
    return (std::sqrt(1 + width * width * tan_squared) - 1) / 2;
}

} // namespace albedo
