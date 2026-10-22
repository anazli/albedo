#include "albedo/light/area_light.hpp"

#include "albedo/core/normal3.hpp"
#include "albedo/core/orthonormal_basis.hpp"

#include <algorithm>
#include <cmath>

namespace albedo {

namespace {

// The cone of directions from a point outside a sphere that hit it.
struct Cone {
    // Unit axis, from the point to the sphere's center.
    Vector3r axis;
    // Distance from the point to the center.
    Real distance;
    // d^2 - r^2: the squared length of a tangent from the point to the sphere.
    Real tangent_squared;
    // cos(theta_max) and 1 - cos(theta_max), the latter computed as
    // sin^2 / (1 + cos) so it keeps its digits when the cone is narrow.
    Real cos_max;
    Real one_minus_cos_max;
};

// The cone `sphere` covers seen from `point`. Nothing when `point` is inside
// (where the sphere's inside, which does not emit, is all there is to see) or
// when the sphere is too small to resolve from there: then 1 - cos(theta_max)
// would underflow and the pdf overflow.
std::optional<Cone> coneTowards(const Sphere& sphere, Point3r point) noexcept
{
    const Vector3r to_center{sphere.center() - point};
    const Real distance_squared{lengthSquared(to_center)};
    const Real radius_squared{sphere.radius() * sphere.radius()};
    if (distance_squared <= radius_squared) {
        return std::nullopt;
    }
    const Real distance{std::sqrt(distance_squared)};
    const Real sin_squared_max{radius_squared / distance_squared};
    const Real cos_max{std::sqrt(1 - sin_squared_max)};
    const Real one_minus_cos_max{sin_squared_max / (1 + cos_max)};
    if (!std::isnormal(one_minus_cos_max)) {
        return std::nullopt;
    }
    return Cone{.axis = to_center / distance,
                .distance = distance,
                .tangent_squared = distance_squared - radius_squared,
                .cos_max = cos_max,
                .one_minus_cos_max = one_minus_cos_max};
}

} // namespace

std::optional<LightSample> AreaLight::sampleIncidentLight(Point3r point, Point2r u) const noexcept
{
    const auto cone{coneTowards(shape, point)};
    if (!cone) {
        return std::nullopt;
    }
    // cos(theta) uniform in [cos_max, 1] gives directions uniform in solid angle.
    // sin^2 is formed from 1 - cos directly so narrow cones keep their precision.
    const Real one_minus_cos{u.x * cone->one_minus_cos_max};
    const Real cos_theta{1 - one_minus_cos};
    const Real sin_squared{one_minus_cos * (2 - one_minus_cos)};
    const Real sin_theta{std::sqrt(sin_squared)};
    const Real phi{2 * pi * u.y};
    const OrthonormalBasis basis{OrthonormalBasis::fromNormal(asNormal(cone->axis))};
    const Vector3r direction{
        basis.toWorld({sin_theta * std::cos(phi), sin_theta * std::sin(phi), cos_theta})};
    // The nearer intersection d cos - sqrt(r^2 - d^2 sin^2), rewritten as
    // (d^2 - r^2) / (d cos + sqrt(...)): near the surface the two terms of the
    // first form are both close to r and cancel, losing up to all of the
    // distance's significant digits; the second form adds instead.
    const Real radius_squared{shape.radius() * shape.radius()};
    const Real root{std::sqrt(
        std::max(Real{0}, radius_squared - cone->distance * cone->distance * sin_squared))};
    return LightSample{.direction = direction,
                       .radiance = radiance,
                       .distance = cone->tangent_squared / (cone->distance * cos_theta + root),
                       .pdf = 1 / (2 * pi * cone->one_minus_cos_max)};
}

Real AreaLight::incidentLightPdf(Point3r point, Vector3r direction) const noexcept
{
    const auto cone{coneTowards(shape, point)};
    if (!cone) {
        return 0;
    }
    if (dot(direction, cone->axis) < cone->cos_max) {
        return 0;
    }
    return 1 / (2 * pi * cone->one_minus_cos_max);
}

} // namespace albedo
