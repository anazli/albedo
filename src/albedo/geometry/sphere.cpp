#include "albedo/geometry/sphere.hpp"

#include "albedo/core/normal3.hpp"
#include "albedo/core/vector3.hpp"

#include <cmath>
#include <format>
#include <stdexcept>
#include <utility>

namespace albedo {

namespace {

// The intersection math squares the radius, so the square must be a normal
// number too: otherwise it overflows to infinity or underflows to zero and every
// ray silently misses.
Real validatedRadius(Real radius)
{
    if (!(radius > 0) || !std::isfinite(radius) || !std::isnormal(radius * radius)) {
        throw std::invalid_argument{std::format(
            "sphere radius must be positive and its square a normal number, got {}", radius)};
    }
    return radius;
}

// A non-finite center would give a non-finite bounding box, which the scene's
// hierarchy rejects.
Point3r validatedCenter(Point3r center)
{
    if (!isFinite(center)) {
        throw std::invalid_argument{
            std::format("sphere center ({}, {}, {}) must be finite", center.x, center.y, center.z)};
    }
    return center;
}

struct Roots {
    Real near{};
    Real far{};
};

// The quadratic a t^2 + 2 h t + c = 0 (a > 0) with its discriminant h^2 - a c,
// which the caller computes in a numerically accurate way.
struct Quadratic {
    Real a{};
    Real h{};
    Real c{};
    Real discriminant{};
};

// Real roots of the quadratic, or none.
//
// Uses the numerically stable form from Press et al., "Numerical Recipes"
// (3rd ed., section 5.6): computing q = -(h + sign(h) sqrt(discriminant)) and the
// roots q / a and c / q avoids subtracting nearly equal numbers, which loses
// most digits of the smaller root when it is tiny compared to the larger one.
std::optional<Roots> solveQuadratic(const Quadratic& quadratic) noexcept
{
    const auto [a, h, c, discriminant]{quadratic};
    if (discriminant < 0) {
        return std::nullopt;
    }
    const Real q{-(h + std::copysign(std::sqrt(discriminant), h))};
    if (q == 0) {
        // Only when h = 0 and a c = 0, i.e. c = 0: both roots are zero.
        return Roots{};
    }
    Real near{q / a};
    Real far{c / q};
    if (near > far) {
        std::swap(near, far);
    }
    return Roots{.near = near, .far = far};
}

} // namespace

Sphere::Sphere(Point3r center, Real radius)
    : sphere_center{validatedCenter(center)}, sphere_radius{validatedRadius(radius)}
{
}

std::optional<ShapeHit> Sphere::intersect(const Ray& ray, Interval t_range) const noexcept
{
    const Vector3r center_to_origin{ray.origin - sphere_center};
    const Real a{lengthSquared(ray.direction)};
    if (!std::isnormal(a)) {
        // Zero, subnormal, infinite or NaN squared length: the direction has too
        // few significant bits (or none) for a meaningful intersection.
        return std::nullopt;
    }
    const Real h{dot(ray.direction, center_to_origin)};
    const Real c{lengthSquared(center_to_origin) - sphere_radius * sphere_radius};

    // h^2 - a c, rewritten as a (r^2 - |l|^2) where l is the vector from the center
    // to the closest point on the ray's line. The textbook form subtracts two huge,
    // nearly equal numbers for a small distant sphere and loses the whole result;
    // see Haines et al., "Precision Improvements for Ray/Sphere Intersection",
    // Ray Tracing Gems (2019), chapter 7.
    const Vector3r closest_offset{center_to_origin - ray.direction * (h / a)};
    const Real discriminant{a * (sphere_radius * sphere_radius - lengthSquared(closest_offset))};

    const auto roots{solveQuadratic({.a = a, .h = h, .c = c, .discriminant = discriminant})};
    if (!roots) {
        return std::nullopt;
    }
    Real t{roots->near};
    if (!t_range.surrounds(t)) {
        t = roots->far;
        if (!t_range.surrounds(t)) {
            return std::nullopt;
        }
    }

    const Point3r point{ray.at(t)};
    const Vector3r outward{point - sphere_center};
    if (!(lengthSquared(outward) > 0)) {
        // The sphere is smaller than the spacing of doubles at its position, so the
        // hit point rounds onto the center and no normal exists: treat it as a miss.
        return std::nullopt;
    }
    // Normalized rather than divided by the radius: for a small, distant sphere the
    // hit point's rounding error is large relative to the radius.
    return ShapeHit{.t = t, .point = point, .normal = asNormal(normalized(outward))};
}

Bounds3r Sphere::bounds() const noexcept
{
    const Vector3r half_extent{sphere_radius, sphere_radius, sphere_radius};
    return {.min = sphere_center - half_extent, .max = sphere_center + half_extent};
}

} // namespace albedo
