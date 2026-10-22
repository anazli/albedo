#include "albedo/scattering/dielectric_lobe.hpp"

#include "albedo/scattering/fresnel.hpp"
#include "albedo/scattering/ggx_distribution.hpp"
#include "albedo/scattering/specular.hpp"

#include <cmath>

namespace albedo {

namespace {

constexpr Vector3r surface_normal{0, 0, 1};

// Matched indices count as smooth whatever the roughness: without a change of
// index the microfacets do not deflect light, which passes straight through.
bool isSmooth(const DielectricLobe& lobe) noexcept
{
    return lobe.alpha < DielectricLobe::smooth_alpha || lobe.eta == 1;
}

Rgb gray(Real value) noexcept
{
    return {.r = value, .g = value, .b = value};
}

// The microfacet normal that refracts `wo` into `wi`, oriented to the viewer's
// side: wo + eta wi is parallel to it exactly when the pair obeys Snell's law
// across it (Walter et al. 2007, eq. 16, with index 1 on wo's side and eta on
// wi's). Nothing when no microfacet can refract the pair: they must lie on
// opposite sides of it, with wo in front.
std::optional<Vector3r> refractionHalfVector(Vector3r wo, Vector3r wi, Real eta) noexcept
{
    const Vector3r sum{wo + wi * eta};
    if (lengthSquared(sum) == 0) {
        return std::nullopt;
    }
    Vector3r m{normalized(sum)};
    if (m.z < 0) {
        m = -m;
    }
    if (dot(wo, m) <= 0 || dot(wi, m) >= 0) {
        return std::nullopt;
    }
    return m;
}

// Squared denominator of the refraction Jacobian and BTDF, (eta wi.m + wo.m)^2.
Real refractionDenominator(Vector3r wo, Vector3r wi, Vector3r m, Real eta) noexcept
{
    const Real denominator{eta * dot(wi, m) + dot(wo, m)};
    return denominator * denominator;
}

std::optional<ScatterSample> sampleSmooth(const DielectricLobe& lobe, Vector3r wo,
                                          Real u_choice) noexcept
{
    const Real eta{lobe.eta};
    const Real reflectance{fresnelDielectric(wo.z, eta)};
    if (u_choice < reflectance) {
        return ScatterSample{.direction = reflect(wo, surface_normal),
                             .weight = gray(1),
                             .pdf = reflectance,
                             .specular = true};
    }
    // Not total internal reflection: then the reflectance is 1 and u_choice < 1.
    const auto wi{refract(wo, surface_normal, eta)};
    if (!wi) {
        return std::nullopt;
    }
    return ScatterSample{.direction = *wi,
                         .weight = gray(1 / (eta * eta)),
                         .pdf = 1 - reflectance,
                         .specular = true};
}

// A reflection that ends below the surface, or a refraction that ends above it,
// cannot leave the microsurface in a single bounce. The two helpers below drop
// them: `pdf` would read such a direction as the other kind of event. Both leave
// the pdf to the caller.

std::optional<ScatterSample> sampleRoughReflection(const GgxDistribution& ggx, Vector3r wo,
                                                   Vector3r m) noexcept
{
    const Vector3r wi{reflect(wo, m)};
    if (wi.z <= 0) {
        return std::nullopt;
    }
    // f cos / pdf reduces to G2 / G1(wo): F cancels against the choice
    // probability, the rest as for a conductor (Heitz 2018, eq. 19).
    return ScatterSample{.direction = wi,
                         .weight = gray(ggx.maskingShadowing(wo, wi) / ggx.masking(wo))};
}

std::optional<ScatterSample> sampleRoughTransmission(const GgxDistribution& ggx, Vector3r wo,
                                                     Vector3r m, Real eta) noexcept
{
    const auto wi{refract(wo, m, eta)};
    if (!wi || wi->z >= 0) {
        return std::nullopt;
    }
    // f cos / pdf: 1 - F cancels against the choice probability, and D, G1(wo)
    // and the Jacobian cancel too, leaving G1(wi) / eta^2.
    return ScatterSample{.direction = *wi, .weight = gray(ggx.masking(*wi) / (eta * eta))};
}

} // namespace

Rgb DielectricLobe::evaluate(Vector3r wo, Vector3r wi) const noexcept
{
    if (isSmooth(*this) || wo.z <= 0 || wi.z == 0) {
        return {};
    }
    const GgxDistribution ggx{alpha};
    if (wi.z > 0) {
        // Reflection, as for a conductor (Walter et al. 2007, eq. 20); both
        // directions are above the surface, so their sum cannot vanish.
        const Vector3r m{normalized(wo + wi)};
        const Real reflectance{fresnelDielectric(dot(wo, m), eta)};
        return gray(reflectance * ggx.density(m) * ggx.maskingShadowing(wo, wi) /
                    (4 * wo.z * wi.z));
    }
    const auto m{refractionHalfVector(wo, wi, eta)};
    if (!m) {
        return {};
    }
    // Walter et al. 2007, eq. 21, with Walter's o (outgoing, towards the viewer)
    // as wo and i as wi, divided through by the index on wo's side.
    // Masking is the separable G1(wo) G1(wi) Walter uses for transmission; the
    // height-correlated form used for reflection does not apply here.
    const Real transmittance{1 - fresnelDielectric(dot(wo, *m), eta)};
    const Real masking{ggx.masking(wo) * ggx.masking(wi)};
    return gray(std::abs(dot(wi, *m)) * dot(wo, *m) * transmittance * ggx.density(*m) * masking /
                (-wi.z * wo.z * refractionDenominator(wo, wi, *m, eta)));
}

std::optional<ScatterSample> DielectricLobe::sample(Vector3r wo, Real u_choice,
                                                    Point2r u) const noexcept
{
    if (wo.z <= 0) {
        return std::nullopt;
    }
    if (isSmooth(*this)) {
        return sampleSmooth(*this, wo, u_choice);
    }
    const GgxDistribution ggx{alpha};
    const Vector3r m{ggx.sampleVisibleNormal(wo, u)};
    const Real facing{dot(wo, m)};
    if (facing <= 0) {
        return std::nullopt;
    }
    std::optional<ScatterSample> result{u_choice < fresnelDielectric(facing, eta)
                                            ? sampleRoughReflection(ggx, wo, m)
                                            : sampleRoughTransmission(ggx, wo, m, eta)};
    if (!result) {
        return std::nullopt;
    }
    result->pdf = pdf(wo, result->direction);
    if (result->pdf <= 0) {
        return std::nullopt;
    }
    return result;
}

Real DielectricLobe::pdf(Vector3r wo, Vector3r wi) const noexcept
{
    if (isSmooth(*this) || wo.z <= 0 || wi.z == 0) {
        return 0;
    }
    const GgxDistribution ggx{alpha};
    if (wi.z > 0) {
        const Vector3r m{normalized(wo + wi)};
        const Real facing{dot(wo, m)};
        // Walter et al. 2007, eq. 14, times the probability of choosing reflection.
        return ggx.visibleNormalPdf(wo, m) * fresnelDielectric(facing, eta) / (4 * facing);
    }
    const auto m{refractionHalfVector(wo, wi, eta)};
    if (!m) {
        return 0;
    }
    // The refraction Jacobian, Walter et al. 2007, eq. 17, times the probability
    // of choosing transmission. Walter samples o given i there, so in that
    // equation his i is our wo and his o our wi: the reverse of eq. 21 above.
    const Real jacobian{eta * eta * std::abs(dot(wi, *m)) / refractionDenominator(wo, wi, *m, eta)};
    return ggx.visibleNormalPdf(wo, *m) * (1 - fresnelDielectric(dot(wo, *m), eta)) * jacobian;
}

} // namespace albedo
