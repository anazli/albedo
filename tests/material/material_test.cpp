#include "albedo/material/material.hpp"

#include "albedo/core/normal3.hpp"
#include "albedo/core/vector3.hpp"
#include "albedo/texture/checker_texture.hpp"
#include "albedo/texture/constant_texture.hpp"
#include "support/expect_near.hpp"
#include "support/require_value.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::expectNear;
using test_support::requireValue;

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
const Material material{DiffuseMaterial{.albedo = ConstantTexture{.color = albedo}}};
const HitRecord hit{
    .t = 1.0, .point = {0.0, 0.0, 0.0}, .normal = {1.0, 0.0, 0.0}, .front_face = true};
const Vector3r wo{normalized(Vector3r{1.0, 0.2, 0.0})};

TEST(MaterialTest, diffuseMaterialReflectsItsAlbedo)
{
    const Scattering scattering{scatteringAt(material, hit)};

    expectNear(scattering.evaluate(wo, normalized(Vector3r{1.0, -0.5, 0.3})), albedo * inv_pi);
}

TEST(MaterialTest, diffuseMaterialTakesItsAlbedoFromTheTextureAtTheHit)
{
    // The hit lies in square (1, 0) of a one-per-unit checker: an odd square.
    constexpr Rgb odd_albedo{0.1, 0.5, 0.9};
    const Material checkered{
        DiffuseMaterial{.albedo = CheckerTexture{.even = {}, .odd = odd_albedo, .frequency = 1.0}}};
    HitRecord textured_hit{hit};
    textured_hit.uv = {1.5, 0.5};

    const Scattering scattering{scatteringAt(checkered, textured_hit)};

    expectNear(scattering.evaluate(wo, normalized(Vector3r{1.0, -0.5, 0.3})), odd_albedo * inv_pi);
}

TEST(MaterialTest, diffuseMaterialDoesNotTransmit)
{
    const Scattering scattering{scatteringAt(material, hit)};

    EXPECT_TRUE(isBlack(scattering.evaluate(wo, normalized(Vector3r{-1.0, 0.5, 0.3}))));
}

TEST(MaterialTest, diffuseMaterialPdfUsesTheHitNormal)
{
    // Only the cosine to the hit normal (+x here) fixes the pdf, so a basis built
    // from any other axis gives a different value.
    const Vector3r wi{normalized(Vector3r{0.3, 0.8, -0.5})};

    EXPECT_NEAR(scatteringAt(material, hit).pdf(wo, wi), wi.x * inv_pi, default_tolerance);
}

TEST(MaterialTest, conductorMaterialIsAMaterial)
{
    static_assert(MaterialLike<ConductorMaterial>);
}

TEST(MaterialTest, smoothConductorMaterialMirrorsAroundTheHitNormal)
{
    // With +x as the normal, the mirror image of wo flips its y and z components.
    const Material conductor{
        ConductorMaterial{.eta = {0.2, 0.9, 1.4}, .k = {3.0, 2.4, 1.6}, .roughness = 0.0}};

    const ScatterSample sample{
        requireValue(scatteringAt(conductor, hit).sample(wo, 0.5, {0.5, 0.5}))};

    expectNear(sample.direction, Vector3r{wo.x, -wo.y, -wo.z});
}

TEST(MaterialTest, conductorRoughnessIsSquaredIntoAlpha)
{
    // Roughness 0.5 gives alpha 0.25: the density at the hit normal (wo and wi
    // both along it) is then F D / 4 with D = 1 / (pi alpha^2).
    const ConductorMaterial conductor{
        .eta = {0.2, 0.9, 1.4}, .k = {3.0, 2.4, 1.6}, .roughness = 0.5};
    const ConductorLobe lobe{.eta = conductor.eta, .k = conductor.k, .alpha = 0.25};
    const Vector3r along_normal{1.0, 0.0, 0.0};

    expectNear(conductor.scatteringAt(hit).evaluate(along_normal, along_normal),
               lobe.evaluate({0.0, 0.0, 1.0}, {0.0, 0.0, 1.0}));
}

TEST(MaterialTest, dielectricMaterialIsAMaterial)
{
    static_assert(MaterialLike<DielectricMaterial>);
}

// sin of the angle between `w` and the axis +-x (the hit normal in these tests).
Real sinToNormal(Vector3r w)
{
    return std::sqrt(w.y * w.y + w.z * w.z);
}

TEST(MaterialTest, dielectricEnteringFromOutsideBendsTowardsTheNormal)
{
    const Material glass{DielectricMaterial{.ior = 1.5, .roughness = 0.0}};
    const Vector3r tilted_wo{normalized(Vector3r{1.0, 0.5, 0.0})};

    const ScatterSample sample{
        requireValue(scatteringAt(glass, hit).sample(tilted_wo, 0.99, {0.5, 0.5}))};

    EXPECT_LT(sample.direction.x, 0.0);
    EXPECT_NEAR(sinToNormal(sample.direction), sinToNormal(tilted_wo) / 1.5, 1e-12);
}

TEST(MaterialTest, dielectricLeavingFromInsideBendsAwayFromTheNormal)
{
    // A back-face hit: the ray is inside the glass, so the index ratio inverts.
    const Material glass{DielectricMaterial{.ior = 1.5, .roughness = 0.0}};
    HitRecord inside_hit{hit};
    inside_hit.front_face = false;
    const Vector3r tilted_wo{normalized(Vector3r{1.0, 0.3, 0.0})};

    const ScatterSample sample{
        requireValue(scatteringAt(glass, inside_hit).sample(tilted_wo, 0.99, {0.5, 0.5}))};

    EXPECT_LT(sample.direction.x, 0.0);
    EXPECT_NEAR(sinToNormal(sample.direction), sinToNormal(tilted_wo) * 1.5, 1e-12);
}

TEST(MaterialTest, dielectricRoughnessIsSquaredIntoAlpha)
{
    // Roughness 0.5 gives alpha 0.25; with wo and wi both along the hit normal
    // only the reflection's F D / 4 remains, and D depends on alpha.
    const DielectricMaterial glass{.ior = 1.5, .roughness = 0.5};
    const DielectricLobe lobe{.eta = 1.5, .alpha = 0.25};
    const Vector3r along_normal{1.0, 0.0, 0.0};

    expectNear(glass.scatteringAt(hit).evaluate(along_normal, along_normal),
               lobe.evaluate({0.0, 0.0, 1.0}, {0.0, 0.0, 1.0}));
}

} // namespace
} // namespace albedo
