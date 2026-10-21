#include "albedo/geometry/shape.hpp"

#include "support/expect_near.hpp"
#include "support/require_value.hpp"

#include <gtest/gtest.h>

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

template <typename... Shapes>
concept CanFormShapeVariant = requires { typename ShapeVariant<Shapes...>; };

TEST(ShapeTest, typeWithoutBoundsIsNotAShape)
{
    static_assert(ShapeLike<Sphere>);
    static_assert(!ShapeLike<ShapeWithoutBounds>);
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

TEST(ShapeTest, boundsDispatchesToTheHeldShape)
{
    const Shape shape{Sphere{Point3r{1.0, 2.0, 3.0}, 0.5}};

    const Bounds3r box{bounds(shape)};

    expectNear(box.min, Point3r{0.5, 1.5, 2.5});
    expectNear(box.max, Point3r{1.5, 2.5, 3.5});
}

} // namespace
} // namespace albedo
