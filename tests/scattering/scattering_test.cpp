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
    const ScatterSample sample{requireValue(scattering.sample(besideNormal(1), 0.5, {0.3, 0.8}))};

    EXPECT_GT(dot(normal, sample.direction), 0.0);
    EXPECT_NEAR(length(sample.direction), 1.0, default_tolerance);
}

TEST(ScatteringTest, sampleIsTheLobeSampleRotatedToWorld)
{
    const OrthonormalBasis basis{OrthonormalBasis::fromNormal(normal)};
    const Vector3r wo{besideNormal(1)};
    const ScatterSample local{
        requireValue(LambertLobe{.albedo = albedo}.sample(basis.toLocal(wo), 0.5, {0.6, 0.1}))};

    const ScatterSample world{requireValue(scattering.sample(wo, 0.5, {0.6, 0.1}))};

    expectNear(world.direction, basis.toWorld(local.direction));
    expectNear(world.weight, local.weight);
    EXPECT_NEAR(world.pdf, local.pdf, default_tolerance);
}

TEST(ScatteringTest, sampleReturnsNothingFromBelowTheNormal)
{
    EXPECT_FALSE(scattering.sample(besideNormal(-1), 0.5, {0.3, 0.8}).has_value());
}

TEST(ScatteringTest, sampleForwardsTheChoiceNumber)
{
    // Smooth glass reflects for a low choice and refracts for a high one.
    const Scattering glass{DielectricLobe{.eta = 1.5, .alpha = 0.0}, normal};
    const Vector3r wo{besideNormal(1)};

    const ScatterSample reflected{requireValue(glass.sample(wo, 0.0, {0.5, 0.5}))};
    const ScatterSample refracted{requireValue(glass.sample(wo, 0.99, {0.5, 0.5}))};

    EXPECT_GT(dot(normal, reflected.direction), 0.0);
    EXPECT_LT(dot(normal, refracted.direction), 0.0);
}

// A shading normal leaning about 63 degrees towards +x off a geometric normal +z.
const Normal3r geometric_up{0.0, 0.0, 1.0};
const Normal3r leaning{normalized(Normal3r{2.0, 0.0, 1.0})};
const SurfaceNormals leaning_normals{.shading = leaning, .geometric = geometric_up};
const Scattering leaning_scattering{LambertLobe{.albedo = albedo}, leaning_normals};
// Above both the geometric surface and the shading frame.
const Vector3r above_both{normalized(Vector3r{0.3, 0.2, 1.0})};
// Above the shading frame but below the geometric surface, and the reverse.
const Vector3r below_surface{normalized(Vector3r{1.0, 0.0, -0.2})};
const Vector3r below_frame{normalized(Vector3r{-1.0, 0.0, 0.2})};

TEST(ScatteringTest, leaningShadingNormalOrientsTheLobe)
{
    EXPECT_NEAR(leaning_scattering.pdf(above_both, above_both), dot(leaning, above_both) * inv_pi,
                default_tolerance);
    expectNear(leaning_scattering.evaluate(above_both, above_both), albedo * inv_pi);
}

TEST(ScatteringTest, directionsCrossingTheActualSurfaceScatterNothing)
{
    for (const Vector3r crossing : {below_surface, below_frame}) {
        EXPECT_TRUE(isBlack(leaning_scattering.evaluate(above_both, crossing)));
        EXPECT_TRUE(isBlack(leaning_scattering.evaluate(crossing, above_both)));
        // Exactly zero: the directions are rejected, not merely unlikely.
        EXPECT_EQ(leaning_scattering.pdf(above_both, crossing), 0.0);
        EXPECT_EQ(leaning_scattering.pdf(crossing, above_both), 0.0);
    }
}

TEST(ScatteringTest, viewerAcrossTheActualSurfaceGetsNoSample)
{
    EXPECT_FALSE(leaning_scattering.sample(below_surface, 0.5, {0.3, 0.8}).has_value());
    EXPECT_FALSE(leaning_scattering.sample(below_frame, 0.5, {0.3, 0.8}).has_value());
}

TEST(ScatteringTest, sampleLeavingThroughTheActualSurfaceIsDiscarded)
{
    // Mirrored about the leaning normal, straight up reflects to (0.8, 0, -0.6):
    // in front of the shading frame, but below the actual surface.
    const Scattering mirror{
        ConductorLobe{.eta = {1.0, 1.0, 1.0}, .k = {1.0, 1.0, 1.0}, .alpha = 0.0}, leaning_normals};

    EXPECT_FALSE(mirror.sample(asVector(geometric_up), 0.5, {0.5, 0.5}).has_value());
}

TEST(ScatteringTest, sampleStayingInFrontOfBothIsKept)
{
    // The centre of the unit square maps onto the shading normal itself.
    const ScatterSample sample{
        requireValue(leaning_scattering.sample(above_both, 0.5, {0.5, 0.5}))};

    EXPECT_GT(dot(sample.direction, leaning), 0.0);
    EXPECT_GT(dot(sample.direction, geometric_up), 0.0);
}

TEST(ScatteringTest, roughGlassTransmitsNothingBackOutThroughTheActualSurface)
{
    // Seen along the shading normal, very rough glass transmits light arriving
    // from `below_frame`, behind the shading frame; but that direction is in
    // front of the actual surface, so the light would come from outside.
    const DielectricLobe lobe{.eta = 1.5, .alpha = 0.6};
    const OrthonormalBasis basis{OrthonormalBasis::fromNormal(leaning)};
    const Scattering glass{lobe, leaning_normals};
    const Vector3r wo{asVector(leaning)};

    ASSERT_FALSE(isBlack(evaluate(lobe, basis.toLocal(wo), basis.toLocal(below_frame))));
    EXPECT_TRUE(isBlack(glass.evaluate(wo, below_frame)));
    EXPECT_EQ(glass.pdf(wo, below_frame), 0.0);
}

TEST(ScatteringTest, smoothGlassReflectionLeavingThroughTheActualSurfaceIsDiscarded)
{
    // As for the mirror: a low choice number reflects, to below the surface.
    const Scattering glass{DielectricLobe{.eta = 1.5, .alpha = 0.0}, leaning_normals};

    EXPECT_FALSE(glass.sample(asVector(geometric_up), 0.0, {0.5, 0.5}).has_value());
}

} // namespace
} // namespace albedo
