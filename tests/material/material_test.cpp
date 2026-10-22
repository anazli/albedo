#include "albedo/material/material.hpp"

#include "albedo/core/normal3.hpp"
#include "albedo/core/vector3.hpp"
#include "support/expect_near.hpp"

#include <gtest/gtest.h>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::expectNear;

// Has no `scatteringAt`, so it must not count as a material.
struct MaterialWithoutScattering {};

template <typename... Materials>
concept CanFormMaterialVariant = requires { typename MaterialVariant<Materials...>; };

TEST(MaterialTest, typeWithoutScatteringIsNotAMaterial)
{
    static_assert(MaterialLike<DiffuseMaterial>);
    static_assert(!MaterialLike<MaterialWithoutScattering>);
}

TEST(MaterialTest, materialVariantRejectsNonMaterialAlternatives)
{
    static_assert(CanFormMaterialVariant<DiffuseMaterial>);
    static_assert(!CanFormMaterialVariant<DiffuseMaterial, MaterialWithoutScattering>);
}

constexpr Rgb albedo{0.7, 0.2, 0.4};
const Material material{DiffuseMaterial{.albedo = albedo}};
const HitRecord hit{
    .t = 1.0, .point = {0.0, 0.0, 0.0}, .normal = {1.0, 0.0, 0.0}, .front_face = true};
const Vector3r wo{normalized(Vector3r{1.0, 0.2, 0.0})};

TEST(MaterialTest, diffuseMaterialReflectsItsAlbedo)
{
    const Scattering scattering{scatteringAt(material, hit)};

    expectNear(scattering.evaluate(wo, normalized(Vector3r{1.0, -0.5, 0.3})), albedo * inv_pi);
}

TEST(MaterialTest, diffuseMaterialDoesNotTransmit)
{
    const Scattering scattering{scatteringAt(material, hit)};

    EXPECT_TRUE(isBlack(scattering.evaluate(wo, normalized(Vector3r{-1.0, 0.5, 0.3}))));
}

TEST(MaterialTest, diffuseMaterialSamplesAroundTheHitNormal)
{
    // Only the cosine to the hit normal (+x here) fixes the pdf, so a basis built
    // from any other axis gives a different value.
    const Vector3r wi{normalized(Vector3r{0.3, 0.8, -0.5})};

    EXPECT_NEAR(scatteringAt(material, hit).pdf(wo, wi), wi.x * inv_pi, default_tolerance);
}

} // namespace
} // namespace albedo
