#include "albedo/core/orthonormal_basis.hpp"

#include "support/expect_near.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::expectNear;

// A deterministic spread of unit normals over the whole sphere, plus the
// cases where naive constructions break down (exactly and nearly ±z).
std::vector<Normal3r> testNormals()
{
    std::vector<Normal3r> normals{
        {0.0, 0.0, 1.0},
        {0.0, 0.0, -1.0},
        normalized(Normal3r{1e-9, 0.0, -1.0}),
        normalized(Normal3r{0.0, 1e-9, -1.0}),
        normalized(Normal3r{1e-9, 1e-9, 1.0}),
        {1.0, 0.0, 0.0},
        {0.0, -1.0, 0.0},
    };
    constexpr int theta_steps{9};
    constexpr int phi_steps{12};
    for (int i{0}; i <= theta_steps; ++i) {
        const Real theta{pi * i / theta_steps};
        for (int j{0}; j < phi_steps; ++j) {
            const Real phi{2 * pi * j / phi_steps};
            normals.push_back({std::sin(theta) * std::cos(phi), std::sin(theta) * std::sin(phi),
                               std::cos(theta)});
        }
    }
    return normals;
}

TEST(OrthonormalBasisTest, normalAxisIsTheInputNormal)
{
    for (const Normal3r& n : testNormals()) {
        const OrthonormalBasis basis{OrthonormalBasis::fromNormal(n)};

        expectNear(basis.normal(), asVector(n));
    }
}

TEST(OrthonormalBasisTest, axesHaveUnitLength)
{
    for (const Normal3r& n : testNormals()) {
        const OrthonormalBasis basis{OrthonormalBasis::fromNormal(n)};

        EXPECT_NEAR(length(basis.tangent()), 1.0, default_tolerance);
        EXPECT_NEAR(length(basis.bitangent()), 1.0, default_tolerance);
        EXPECT_NEAR(length(basis.normal()), 1.0, default_tolerance);
    }
}

TEST(OrthonormalBasisTest, axesAreMutuallyOrthogonal)
{
    for (const Normal3r& n : testNormals()) {
        const OrthonormalBasis basis{OrthonormalBasis::fromNormal(n)};

        EXPECT_NEAR(dot(basis.tangent(), basis.bitangent()), 0.0, default_tolerance);
        EXPECT_NEAR(dot(basis.tangent(), basis.normal()), 0.0, default_tolerance);
        EXPECT_NEAR(dot(basis.bitangent(), basis.normal()), 0.0, default_tolerance);
    }
}

TEST(OrthonormalBasisTest, isRightHanded)
{
    for (const Normal3r& n : testNormals()) {
        const OrthonormalBasis basis{OrthonormalBasis::fromNormal(n)};

        expectNear(cross(basis.tangent(), basis.bitangent()), basis.normal());
    }
}

TEST(OrthonormalBasisTest, toLocalMapsNormalToPositiveZ)
{
    for (const Normal3r& n : testNormals()) {
        const OrthonormalBasis basis{OrthonormalBasis::fromNormal(n)};

        expectNear(basis.toLocal(asVector(n)), Vector3r{0.0, 0.0, 1.0});
    }
}

TEST(OrthonormalBasisTest, toWorldInvertsToLocal)
{
    constexpr Vector3r v{0.3, -1.2, 2.5};

    for (const Normal3r& n : testNormals()) {
        const OrthonormalBasis basis{OrthonormalBasis::fromNormal(n)};

        expectNear(basis.toWorld(basis.toLocal(v)), v);
    }
}

TEST(OrthonormalBasisTest, toLocalInvertsToWorld)
{
    constexpr Vector3r v{0.3, -1.2, 2.5};

    for (const Normal3r& n : testNormals()) {
        const OrthonormalBasis basis{OrthonormalBasis::fromNormal(n)};

        expectNear(basis.toLocal(basis.toWorld(v)), v);
    }
}

TEST(OrthonormalBasisDeathTest, fromNormalAssertsOnNonUnitNormal)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(OrthonormalBasis::fromNormal({0.0, 0.0, 2.0})), "");
}

} // namespace
} // namespace albedo
