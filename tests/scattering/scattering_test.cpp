#include "albedo/scattering/scattering.hpp"

#include "albedo/core/normal3.hpp"
#include "albedo/core/orthonormal_basis.hpp"
#include "albedo/core/vector3.hpp"
#include "support/expect_near.hpp"
#include "support/require_value.hpp"

#include <gtest/gtest.h>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::exact_tolerance;
using test_support::expectNear;
using test_support::requireValue;

constexpr Rgb albedo{0.6, 0.3, 0.1};
// A normal far from every axis, so a missing world/local conversion shows.
const Normal3r normal{normalized(Normal3r{-0.2, 0.9, 0.4})};
const Scattering scattering{LambertLobe{.albedo = albedo}, normal};

// A unit direction on the side of `normal` given by `sign`, tilted away from it.
Vector3r besideNormal(Real sign)
{
    return normalized(asVector(normal) * sign + Vector3r{0.5, 0.0, -0.3});
}

// Above the normal but with world z < 0, and below it with world z > 0, so a
// lobe fed world directions as if they were local would get the side wrong.
const Vector3r above_with_negative_z{normalized(asVector(normal) + Vector3r{0.0, 0.0, -0.8})};
const Vector3r below_with_positive_z{normalized(-asVector(normal) + Vector3r{0.0, 0.0, 0.8})};

TEST(ScatteringTest, evaluatesReflectionAroundTheWorldNormal)
{
    expectNear(scattering.evaluate(besideNormal(1), above_with_negative_z), albedo * inv_pi);
}

TEST(ScatteringTest, evaluatesToBlackAcrossTheWorldNormal)
{
    EXPECT_TRUE(isBlack(scattering.evaluate(besideNormal(1), below_with_positive_z)));
}

TEST(ScatteringTest, pdfIsZeroAcrossTheWorldNormal)
{
    EXPECT_NEAR(scattering.pdf(besideNormal(1), below_with_positive_z), 0.0, exact_tolerance);
}

TEST(ScatteringTest, pdfIsZeroFromBelowTheWorldNormal)
{
    EXPECT_NEAR(scattering.pdf(below_with_positive_z, besideNormal(1)), 0.0, exact_tolerance);
}

TEST(ScatteringTest, pdfUsesCosineToTheWorldNormal)
{
    const Vector3r wi{above_with_negative_z};

    EXPECT_NEAR(scattering.pdf(besideNormal(1), wi), dot(normal, wi) * inv_pi, default_tolerance);
}

TEST(ScatteringTest, sampleReturnsWorldDirectionAboveTheNormal)
{
    const ScatterSample sample{requireValue(scattering.sample(besideNormal(1), {0.3, 0.8}))};

    EXPECT_GT(dot(normal, sample.direction), 0.0);
    EXPECT_NEAR(length(sample.direction), 1.0, default_tolerance);
}

TEST(ScatteringTest, sampleIsTheLobeSampleRotatedToWorld)
{
    const OrthonormalBasis basis{OrthonormalBasis::fromNormal(normal)};
    const Vector3r wo{besideNormal(1)};
    const ScatterSample local{
        requireValue(LambertLobe{.albedo = albedo}.sample(basis.toLocal(wo), {0.6, 0.1}))};

    const ScatterSample world{requireValue(scattering.sample(wo, {0.6, 0.1}))};

    expectNear(world.direction, basis.toWorld(local.direction));
    expectNear(world.weight, local.weight);
    EXPECT_NEAR(world.pdf, local.pdf, default_tolerance);
}

TEST(ScatteringTest, sampleReturnsNothingFromBelowTheNormal)
{
    EXPECT_FALSE(scattering.sample(besideNormal(-1), {0.3, 0.8}).has_value());
}

} // namespace
} // namespace albedo
