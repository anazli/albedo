#include "albedo/geometry/triangle.hpp"

#include "albedo/core/vector3.hpp"

#include <cassert>
#include <cmath>
#include <format>
#include <stdexcept>
#include <utility>

namespace albedo {

namespace {

// The ray-triangle test follows Woop, Benthin and Wald, "Watertight Ray/Triangle
// Intersection", Journal of Computer Graphics Techniques 2(1), 2013. It moves the
// ray origin to zero and shears space so the ray runs along +z; the hit test then
// becomes a 2D point-in-triangle test at the origin. Each edge function depends
// only on that edge's two vertices, so triangles sharing an edge compute exactly
// negated values there and no ray can slip between them.

// Throws unless the triangle is non-degenerate and its normal is computable.
Normal3r frontNormal(Point3r p0, Point3r p1, Point3r p2)
{
    if (!isFinite(p0) || !isFinite(p1) || !isFinite(p2)) {
        throw std::invalid_argument{"triangle vertices must have finite coordinates"};
    }
    if (!formsTriangle(p0, p1, p2)) {
        throw std::invalid_argument{
            std::format("triangle ({}, {}, {}), ({}, {}, {}), ({}, {}, {}) has no computable area",
                        p0.x, p0.y, p0.z, p1.x, p1.y, p1.z, p2.x, p2.y, p2.z)};
    }
    return asNormal(normalized(cross(p1 - p0, p2 - p0)));
}

Real component(Vector3r v, int axis) noexcept
{
    assert(axis >= 0 && axis < 3 && "axis must be 0, 1 or 2");
    switch (axis) {
    case 0:
        return v.x;
    case 1:
        return v.y;
    default:
        return v.z;
    }
}

int largestAxis(Vector3r v) noexcept
{
    const Vector3r magnitude{std::abs(v.x), std::abs(v.y), std::abs(v.z)};
    if (magnitude.x > magnitude.y) {
        return magnitude.x > magnitude.z ? 0 : 2;
    }
    return magnitude.y > magnitude.z ? 1 : 2;
}

// The permutation and shear that map the ray direction onto +z.
struct RaySpace {
    int kx{};
    int ky{};
    int kz{};
    Real shear_x{};
    Real shear_y{};
    Real scale_z{};

    // `offset` (a vertex minus the ray origin) in ray space.
    [[nodiscard]] Vector3r transformed(Vector3r offset) const noexcept
    {
        const Real z{component(offset, kz)};
        return {component(offset, kx) - shear_x * z, component(offset, ky) - shear_y * z,
                scale_z * z};
    }
};

// Precondition: `direction` is not zero.
RaySpace raySpaceAlong(Vector3r direction) noexcept
{
    const int kz{largestAxis(direction)};
    const Real dz{component(direction, kz)};
    assert(dz != 0 && "the ray direction must not be zero");
    int kx{(kz + 1) % 3};
    int ky{(kx + 1) % 3};
    // Swapping x and y for a ray along -z keeps the winding, and so the sign of
    // the edge functions, the same as seen along +z.
    if (dz < 0) {
        std::swap(kx, ky);
    }
    return {.kx = kx,
            .ky = ky,
            .kz = kz,
            .shear_x = component(direction, kx) / dz,
            .shear_y = component(direction, ky) / dz,
            .scale_z = 1 / dz};
}

// Twice the signed area of the 2D triangle (origin, a, b), as seen along +z.
// edgeFunction(b, a) must be the exact negation of edgeFunction(a, b), which only
// holds when each product is rounded on its own: CMake builds this file with
// floating-point contraction off so the compiler cannot fuse them into an FMA.
Real edgeFunction(Vector3r a, Vector3r b) noexcept
{
    return b.x * a.y - b.y * a.x;
}

} // namespace

Triangle::Triangle(Point3r p0, Point3r p1, Point3r p2)
    : corners{p0, p1, p2}, front_normal{frontNormal(p0, p1, p2)}
{
}

std::optional<ShapeHit> Triangle::intersect(const Ray& ray, Interval t_range) const noexcept
{
    if (!std::isnormal(lengthSquared(ray.direction))) {
        // As for spheres: too few significant bits, or none, for a meaningful hit.
        return std::nullopt;
    }
    const RaySpace space{raySpaceAlong(ray.direction)};
    const Vector3r a{space.transformed(corners[0] - ray.origin)};
    const Vector3r b{space.transformed(corners[1] - ray.origin)};
    const Vector3r c{space.transformed(corners[2] - ray.origin)};

    // Each weight belongs to the vertex opposite its edge. Zero counts as inside,
    // so a ray through an edge hits both triangles sharing it. Written as a test
    // for inside rather than outside so that a NaN weight (from a non-finite origin
    // or overflowing coordinates) is a miss.
    const Real u{edgeFunction(b, c)};
    const Real v{edgeFunction(c, a)};
    const Real w{edgeFunction(a, b)};
    const bool inside{(u >= 0 && v >= 0 && w >= 0) || (u <= 0 && v <= 0 && w <= 0)};
    if (!inside) {
        return std::nullopt;
    }
    const Real determinant{u + v + w};
    if (determinant == 0) {
        // The ray runs parallel to the triangle's plane.
        return std::nullopt;
    }
    const Real t{(u * a.z + v * b.z + w * c.z) / determinant};
    if (!t_range.surrounds(t)) {
        return std::nullopt;
    }

    // Interpolating the vertices keeps the point on the plane, which `ray.at(t)`
    // only approximates.
    const Real b1{v / determinant};
    const Real b2{w / determinant};
    const Point3r point{corners[0] + (corners[1] - corners[0]) * b1 +
                        (corners[2] - corners[0]) * b2};
    return ShapeHit{.t = t, .point = point, .normal = front_normal};
}

Bounds3r Triangle::bounds() const noexcept
{
    return merged(merged(merged(Bounds3r{}, corners[0]), corners[1]), corners[2]);
}

bool formsTriangle(Point3r p0, Point3r p1, Point3r p2) noexcept
{
    if (!isFinite(p0) || !isFinite(p1) || !isFinite(p2)) {
        return false;
    }
    // Also rejects edges long enough to overflow the cross product to infinity.
    return std::isnormal(lengthSquared(cross(p1 - p0, p2 - p0)));
}

} // namespace albedo
