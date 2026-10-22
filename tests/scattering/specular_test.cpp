#include "albedo/scattering/specular.hpp"

#include "albedo/core/vector3.hpp"
#include "support/expect_near.hpp"

#include <gtest/gtest.h>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::expectNear;

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

} // namespace
} // namespace albedo
