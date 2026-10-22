#include "albedo/scattering/fresnel.hpp"

#include <algorithm>
#include <cassert>
#include <complex>

namespace albedo {

namespace {

using Complex = std::complex<Real>;

// The Fresnel equations for amplitude reflection coefficients (Hecht,
// "Optics", sec. 4.6), averaged over both polarizations. With a complex index
// the same equations describe absorbing media; std::sqrt returns the root with
// non-negative real part, the physically meaningful one for the transmitted wave.
Real reflectance(Real cos_theta_i, Complex eta) noexcept
{
    const Real sin_squared_i{1 - cos_theta_i * cos_theta_i};
    const Complex cos_theta_t{std::sqrt(Complex{1} - sin_squared_i / (eta * eta))};
    const Complex r_parallel{(eta * cos_theta_i - cos_theta_t) / (eta * cos_theta_i + cos_theta_t)};
    const Complex r_perpendicular{(cos_theta_i - eta * cos_theta_t) /
                                  (cos_theta_i + eta * cos_theta_t)};
    return (std::norm(r_parallel) + std::norm(r_perpendicular)) / 2;
}

} // namespace

Real fresnelDielectric(Real cos_theta_i, Real eta) noexcept
{
    assert(eta > 0 && "relative index of refraction must be positive");
    if (cos_theta_i < 0) {
        cos_theta_i = -cos_theta_i;
        eta = 1 / eta;
    }
    cos_theta_i = std::min(cos_theta_i, Real{1});
    // No interface: also avoids reading grazing incidence as the critical angle.
    if (eta == 1) {
        return 0;
    }
    // Snell's law: no transmitted wave exists past the critical angle.
    const Real sin_squared_t{(1 - cos_theta_i * cos_theta_i) / (eta * eta)};
    if (sin_squared_t >= 1) {
        return 1;
    }
    return reflectance(cos_theta_i, Complex{eta});
}

Rgb fresnelConductor(Real cos_theta_i, Rgb eta, Rgb k) noexcept
{
    assert(eta.r > 0 && eta.g > 0 && eta.b > 0 && "conductor index must be positive");
    assert(k.r >= 0 && k.g >= 0 && k.b >= 0 && "conductor absorption must not be negative");
    // Cosines from dot products of unit vectors can leave [0, 1] by rounding.
    cos_theta_i = std::clamp(cos_theta_i, Real{0}, Real{1});
    return {.r = reflectance(cos_theta_i, {eta.r, k.r}),
            .g = reflectance(cos_theta_i, {eta.g, k.g}),
            .b = reflectance(cos_theta_i, {eta.b, k.b})};
}

} // namespace albedo
