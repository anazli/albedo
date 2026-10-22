#include "albedo/scattering/specular.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace albedo {

std::optional<Vector3r> refract(Vector3r w, Vector3r n, Real eta) noexcept
{
    assert(eta > 0 && "relative index of refraction must be positive");
    const Real cos_i{dot(w, n)};
    assert(cos_i > 0 && "w must lie on the normal's side");
    const Real sin_squared_t{std::max(Real{0}, 1 - cos_i * cos_i) / (eta * eta)};
    if (sin_squared_t >= 1) {
        return std::nullopt;
    }
    const Real cos_t{std::sqrt(1 - sin_squared_t)};
    // Snell's law in vector form: the tangential part of w shrinks by 1 / eta
    // and flips, and the normal part becomes -cos_t.
    return -w / eta + n * (cos_i / eta - cos_t);
}

} // namespace albedo
