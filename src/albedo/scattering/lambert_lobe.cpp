#include "albedo/scattering/lambert_lobe.hpp"

#include "albedo/sampling/warps.hpp"

namespace albedo {

namespace {

// Lambertian surfaces only reflect: both directions must leave the front side.
bool isReflection(Vector3r wo, Vector3r wi) noexcept
{
    return wo.z > 0 && wi.z > 0;
}

} // namespace

Rgb LambertLobe::evaluate(Vector3r wo, Vector3r wi) const noexcept
{
    return isReflection(wo, wi) ? albedo * inv_pi : Rgb{};
}

std::optional<ScatterSample> LambertLobe::sample(Vector3r wo, Point2r u) const noexcept
{
    if (wo.z <= 0) {
        return std::nullopt;
    }
    const Vector3r wi{sampleCosineHemisphere(u)};
    // A boundary `u` can give a direction in the tangent plane, where the pdf is
    // zero and evaluate is black. Dropping that measure-zero event adds no bias
    // and keeps every returned sample consistent with evaluate and pdf.
    if (wi.z <= 0) {
        return std::nullopt;
    }
    return ScatterSample{.direction = wi, .weight = albedo, .pdf = cosineHemispherePdf(wi.z)};
}

Real LambertLobe::pdf(Vector3r wo, Vector3r wi) noexcept
{
    return wo.z > 0 ? cosineHemispherePdf(wi.z) : Real{0};
}

} // namespace albedo
