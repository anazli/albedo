#include "albedo/geometry/shape.hpp"

#include "albedo/core/transform.hpp"
#include "albedo/geometry/mesh_geometry.hpp"
#include "albedo/geometry/triangle_mesh.hpp"
#include "support/expect_near.hpp"
#include "support/require_value.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <optional>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::expectNear;
using test_support::requireValue;

// Has `intersect` but no `bounds`, so it must not count as a shape.
struct ShapeWithoutBounds {
    [[nodiscard]] static std::optional<ShapeHit> intersect(const Ray& /*ray*/,
                                                           Interval /*t_range*/) noexcept
    {
        return std::nullopt;
    }
};

// Has `intersect` and `bounds` but no `textureCoordinatesAt`, so it must not
// count as a shape either.
struct ShapeWithoutTextureCoordinates {
    [[nodiscard]] static std::optional<ShapeHit> intersect(const Ray& /*ray*/,
                                                           Interval /*t_range*/) noexcept
    {
        return std::nullopt;
    }

    [[nodiscard]] static Bounds3r bounds() noexcept { return {}; }
};

template <typename... Shapes>
concept CanFormShapeVariant = requires { typename ShapeVariant<Shapes...>; };

TEST(ShapeTest, typeWithoutBoundsIsNotAShape)
{
    static_assert(ShapeLike<Sphere>);
    static_assert(ShapeLike<Triangle>);
    static_assert(ShapeLike<MeshInstance>);
    static_assert(!ShapeLike<ShapeWithoutBounds>);
}

TEST(ShapeTest, typeWithoutTextureCoordinatesIsNotAShape)
{
    static_assert(!ShapeLike<ShapeWithoutTextureCoordinates>);
}

TEST(ShapeTest, textureCoordinatesOfASphereComeFromTheHitNormal)
{
    // Spheres compute them on request, from the normal: +z is a quarter turn round.
    const Shape shape{Sphere{Point3r{0.0, 0.0, -3.0}, 1.0}};
    const ShapeHit hit{.t = 2.0, .point = {0.0, 0.0, -2.0}, .normal = {0.0, 0.0, 1.0}};

    expectNear(textureCoordinatesAt(shape, hit), Point2r{0.25, 0.5});
}

TEST(ShapeTest, textureCoordinatesOfATriangleAreThoseOfTheHit)
{
    const Shape shape{
        Triangle{Point3r{-1.0, -1.0, -3.0}, Point3r{1.0, -1.0, -3.0}, Point3r{0.0, 1.0, -3.0}}};
    const ShapeHit hit{
        .t = 3.0, .point = {0.0, 0.0, -3.0}, .normal = {0.0, 0.0, 1.0}, .uv = {0.3, 0.6}};

    expectNear(textureCoordinatesAt(shape, hit), Point2r{0.3, 0.6});
}

TEST(ShapeTest, shapeVariantRejectsNonShapeAlternatives)
{
    static_assert(CanFormShapeVariant<Sphere>);
    static_assert(!CanFormShapeVariant<Sphere, ShapeWithoutBounds>);
}

TEST(ShapeTest, intersectDispatchesToTheHeldShape)
{
    const Shape shape{Sphere{Point3r{0.0, 0.0, -3.0}, 1.0}};
    const Ray ray{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 0.0, -1.0}};

    const ShapeHit hit{requireValue(intersect(shape, ray, Interval{.min = 0.0, .max = 10.0}))};

    EXPECT_NEAR(hit.t, 2.0, default_tolerance);
}

TEST(ShapeTest, intersectDispatchesToAHeldTriangle)
{
    const Shape shape{
        Triangle{Point3r{-1.0, -1.0, -3.0}, Point3r{1.0, -1.0, -3.0}, Point3r{0.0, 1.0, -3.0}}};
    const Ray ray{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 0.0, -1.0}};

    const ShapeHit hit{requireValue(intersect(shape, ray, Interval{.min = 0.0, .max = 10.0}))};

    EXPECT_NEAR(hit.t, 3.0, default_tolerance);
}

TEST(ShapeTest, intersectDispatchesToAHeldMeshInstance)
{
    const auto geometry{std::make_shared<const MeshGeometry>(
        TriangleMesh{{{-1.0, -1.0, -3.0}, {1.0, -1.0, -3.0}, {0.0, 1.0, -3.0}}, {{0, 1, 2}}})};
    const Shape shape{MeshInstance{geometry, Transform::translation({0.0, 0.0, -1.0})}};
    const Ray ray{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 0.0, -1.0}};

    const ShapeHit hit{requireValue(intersect(shape, ray, Interval{.min = 0.0, .max = 10.0}))};

    EXPECT_NEAR(hit.t, 4.0, default_tolerance);
}

TEST(ShapeTest, textureCoordinatesOfAMeshInstanceComeFromItsMesh)
{
    // A textured square at object-space depth -3, doubled by the instance; the ray
    // hits object-space (0.5, -0.5), three quarters across in u and a quarter up in v.
    const auto geometry{std::make_shared<const MeshGeometry>(
        TriangleMesh{{{-1.0, -1.0, -3.0}, {1.0, -1.0, -3.0}, {1.0, 1.0, -3.0}, {-1.0, 1.0, -3.0}},
                     {{0, 1, 2}, {0, 2, 3}},
                     {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}},
                     {{0, 1, 2}, {0, 2, 3}}})};
    const Shape shape{MeshInstance{geometry, Transform::scaling({2.0, 2.0, 2.0})}};
    const Ray ray{.origin = {1.0, -1.0, 0.0}, .direction = {0.0, 0.0, -1.0}};
    const ShapeHit hit{requireValue(intersect(shape, ray, Interval{.min = 0.0, .max = 10.0}))};

    expectNear(textureCoordinatesAt(shape, hit), Point2r{0.75, 0.25});
}

TEST(ShapeTest, boundsDispatchesToTheHeldShape)
{
    const Shape shape{Sphere{Point3r{1.0, 2.0, 3.0}, 0.5}};

    const Bounds3r box{bounds(shape)};

    expectNear(box.min, Point3r{0.5, 1.5, 2.5});
    expectNear(box.max, Point3r{1.5, 2.5, 3.5});
}

TEST(ShapeTest, boundsDispatchesToAHeldTriangle)
{
    const Shape shape{
        Triangle{Point3r{-1.0, 0.0, 2.0}, Point3r{1.0, -2.0, 3.0}, Point3r{0.0, 1.0, 4.0}}};

    const Bounds3r box{bounds(shape)};

    expectNear(box.min, Point3r{-1.0, -2.0, 2.0});
    expectNear(box.max, Point3r{1.0, 1.0, 4.0});
}

} // namespace
} // namespace albedo
