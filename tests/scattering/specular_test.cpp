#include "albedo/scattering/specular.hpp"

#include "albedo/core/vector3.hpp"
#include "support/expect_near.hpp"
#include "support/require_value.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::expectNear;
using test_support::requireValue;

TEST(SpecularTest, reflectMirrorsAboutTheNormal)
{
    expectNear(reflect(normalized(Vector3r{0.6, 0.0, 0.8}), {0.0, 0.0, 1.0}),
               normalized(Vector3r{-0.6, 0.0, 0.8}));
}

TEST(SpecularTest, reflectIsUsableAtCompileTime)
{
    static_assert(reflect({1.0, 0.0, 1.0}, {0.0, 0.0, 1.0}) == Vector3r{-1.0, 0.0, 1.0});
}

TEST(SpecularTest, reflectAboutATiltedNormalKeepsTheAngle)
{
    const Vector3r n{normalized(Vector3r{0.2, -0.3, 0.9})};
    const Vector3r w{normalized(Vector3r{-0.4, 0.5, 0.7})};

    const Vector3r reflected{reflect(w, n)};

    EXPECT_NEAR(dot(reflected, n), dot(w, n), default_tolerance);
    EXPECT_NEAR(length(reflected), 1.0, default_tolerance);
    // w, n and the reflection lie in one plane.
    EXPECT_NEAR(dot(cross(w, n), reflected), 0.0, default_tolerance);
}

TEST(SpecularTest, refractAtNormalIncidenceGoesStraightThrough)
{
    const Vector3r refracted{requireValue(refract({0.0, 0.0, 1.0}, {0.0, 0.0, 1.0}, 1.5))};

    expectNear(refracted, Vector3r{0.0, 0.0, -1.0});
}

TEST(SpecularTest, refractFollowsSnellsLaw)
{
    // sin(theta_t) = sin(theta_i) / eta, on the far side of the normal, in the
    // plane of w and n, turned the other way round the normal from w.
    constexpr Real eta{1.5};
    const Vector3r w{normalized(Vector3r{0.6, -0.2, 0.7})};
    const Vector3r n{0.0, 0.0, 1.0};

    const Vector3r refracted{requireValue(refract(w, n, eta))};

    const Real sin_i{std::sqrt(w.x * w.x + w.y * w.y)};
    const Real sin_t{std::sqrt(refracted.x * refracted.x + refracted.y * refracted.y)};
    EXPECT_NEAR(length(refracted), 1.0, default_tolerance);
    EXPECT_LT(refracted.z, 0.0);
    EXPECT_NEAR(sin_t, sin_i / eta, default_tolerance);
    EXPECT_NEAR(dot(cross(w, n), refracted), 0.0, default_tolerance);
    EXPECT_LT(refracted.x * w.x + refracted.y * w.y, 0.0);
}

TEST(SpecularTest, refractAboutATiltedNormalFollowsSnellsLaw)
{
    constexpr Real eta{1.3};
    const Vector3r n{normalized(Vector3r{0.3, 0.4, 0.8})};
    const Vector3r w{normalized(Vector3r{-0.2, 0.5, 0.6})};

    const Vector3r refracted{requireValue(refract(w, n, eta))};

    const Real cos_i{dot(w, n)};
    const Real cos_t{-dot(refracted, n)};
    EXPECT_NEAR(std::sqrt(1 - cos_t * cos_t), std::sqrt(1 - cos_i * cos_i) / eta, 1e-12);
    EXPECT_GT(cos_t, 0.0);
    EXPECT_NEAR(dot(cross(w, n), refracted), 0.0, default_tolerance);
    // The tangential parts point opposite ways.
    EXPECT_LT(dot(w - n * cos_i, refracted + n * cos_t), 0.0);
}

TEST(SpecularTest, matchedIndicesDoNotBend)
{
    const Vector3r w{normalized(Vector3r{0.6, -0.2, 0.7})};

    expectNear(requireValue(refract(w, {0.0, 0.0, 1.0}, 1.0)), -w);
}

TEST(SpecularTest, refractReturnsNothingUnderTotalInternalReflection)
{
    // Leaving glass at 60 degrees, beyond the critical angle of about 41.8.
    const Vector3r w{std::sin(pi / 3), 0.0, std::cos(pi / 3)};

    EXPECT_FALSE(refract(w, {0.0, 0.0, 1.0}, 1 / 1.5).has_value());
}

TEST(SpecularDeathTest, refractRejectsNonPositiveIndexRatio)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(refract({0.0, 0.0, 1.0}, {0.0, 0.0, 1.0}, 0.0)), "");
}

TEST(SpecularDeathTest, refractRejectsADirectionBehindTheNormal)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(refract({0.0, 0.0, -1.0}, {0.0, 0.0, 1.0}, 1.5)), "");
}

} // namespace
} // namespace albedo
