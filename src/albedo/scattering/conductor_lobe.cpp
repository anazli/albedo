#include "albedo/scattering/conductor_lobe.hpp"

#include "albedo/scattering/fresnel.hpp"
#include "albedo/scattering/ggx_distribution.hpp"
#include "albedo/scattering/specular.hpp"

namespace albedo {

namespace {

bool isSmooth(Real alpha) noexcept
{
    return alpha < ConductorLobe::smooth_alpha;
}

} // namespace

Rgb ConductorLobe::evaluate(Vector3r wo, Vector3r wi) const noexcept
{
    if (isSmooth(alpha) || wo.z <= 0 || wi.z <= 0) {
        return {};
    }
    // Both directions are above the surface, so their sum cannot vanish.
    const Vector3r m{normalized(wo + wi)};
    // The Torrance-Sparrow form of a microfacet BRDF (Walter et al. 2007, eq. 20):
    // F D G / (4 |wo.n| |wi.n|).
    const GgxDistribution ggx{alpha};
    const Rgb fresnel{fresnelConductor(dot(wo, m), eta, k)};
    return fresnel * (ggx.density(m) * ggx.maskingShadowing(wo, wi) / (4 * wo.z * wi.z));
}

std::optional<ScatterSample> ConductorLobe::sample(Vector3r wo, Real /*u_choice*/,
                                                   Point2r u) const noexcept
{
    if (wo.z <= 0) {
        return std::nullopt;
    }
    if (isSmooth(alpha)) {
        return ScatterSample{.direction = reflect(wo, {0, 0, 1}),
                             .weight = fresnelConductor(wo.z, eta, k),
                             .pdf = 1,
                             .specular = true};
    }
    const GgxDistribution ggx{alpha};
    const Vector3r m{ggx.sampleVisibleNormal(wo, u)};
    const Vector3r wi{reflect(wo, m)};
    const Real sample_pdf{pdf(wo, wi)};
    if (wi.z <= 0 || sample_pdf <= 0) {
        return std::nullopt;
    }
    // With visible-normal sampling, D and most of the Jacobian cancel: the
    // weight f cos / pdf reduces to F G2 / G1 (Heitz 2018, eq. 19).
    const Rgb fresnel{fresnelConductor(dot(wo, m), eta, k)};
    return ScatterSample{.direction = wi,
                         .weight = fresnel * (ggx.maskingShadowing(wo, wi) / ggx.masking(wo)),
                         .pdf = sample_pdf};
}

Real ConductorLobe::pdf(Vector3r wo, Vector3r wi) const noexcept
{
    if (isSmooth(alpha) || wo.z <= 0 || wi.z <= 0) {
        return 0;
    }
    // Both directions are above the surface, so their sum cannot vanish.
    const Vector3r m{normalized(wo + wi)};
    const Real facing{dot(wo, m)};
    if (facing <= 0) {
        return 0;
    }
    // Reflecting about m maps normal densities to direction densities with
    // Jacobian 1 / (4 wo.m) (Walter et al. 2007, eq. 14).
    return GgxDistribution{alpha}.visibleNormalPdf(wo, m) / (4 * facing);
}

} // namespace albedo
