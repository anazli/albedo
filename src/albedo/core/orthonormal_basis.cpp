#include "albedo/core/orthonormal_basis.hpp"

#include "albedo/core/real.hpp"

#include <cassert>
#include <cmath>

namespace albedo {

namespace {

// Loose enough to accept normals that were normalized in float precision.
constexpr Real unit_length_tolerance{1e-6};

} // namespace

OrthonormalBasis OrthonormalBasis::fromNormal(Normal3r n) noexcept
{
    assert(std::abs(length(asVector(n)) - 1) < unit_length_tolerance &&
           "fromNormal requires a unit normal");

    // Branchless construction from Duff et al., "Building an Orthonormal Basis,
    // Revisited", JCGT 6(1), 2017. Copying the sign of z keeps the denominator
    // away from zero, so it stays accurate for normals near -z, where Frisvad's
    // original method loses precision.
    const Real sign{std::copysign(Real{1}, n.z)};
    const Real a{-1 / (sign + n.z)};
    const Real b{n.x * n.y * a};

    OrthonormalBasis basis;
    basis.tangent_axis = {1 + sign * n.x * n.x * a, sign * b, -sign * n.x};
    basis.bitangent_axis = {b, sign + n.y * n.y * a, -n.y};
    basis.normal_axis = asVector(n);
    return basis;
}

} // namespace albedo
