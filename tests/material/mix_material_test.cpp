#include "albedo/material/mix_material.hpp"

#include "albedo/core/normal3.hpp"
#include "albedo/core/vector3.hpp"
#include "albedo/texture/constant_texture.hpp"
#include "support/expect_near.hpp"
#include "support/require_value.hpp"

#include <gtest/gtest.h>

#include <limits>

namespace albedo {
namespace {

using test_support::expectNear;
using test_support::requireValue;

constexpr Rgb first_color{0.7, 0.2, 0.4};
constexpr Rgb second_color{0.1, 0.5, 0.3};
const DiffuseMaterial first_material{.albedo = ConstantTexture{.color = first_color}};
const DiffuseMaterial second_material{.albedo = ConstantTexture{.color = second_color}};

// A hit on the noise lattice, where clouds weigh 0 and marble with scale 1
// weighs 0.5.
const HitRecord lattice_hit{
    .t = 1.0, .point = {0.0, 2.0, -3.0}, .normal = {0.0, 0.0, 1.0}, .front_face = true};
const Vector3r up{0.0, 0.0, 1.0};

// The diffuse reflectance the mix scatters with at `hit`.
Rgb reflectance(const MixMaterial& mix, const HitRecord& hit = lattice_hit)
{
    return mix.scatteringAt(hit).evaluate(up, up) * pi;
}

TEST(MixMaterialTest, usesTheFirstMaterialBelowTheThreshold)
{
    const MixMaterial mix{.first = first_material,
                          .second = second_material,
                          .pattern = {.scale = 1},
                          .threshold = 0.5};

    expectNear(reflectance(mix), first_color);
}

TEST(MixMaterialTest, usesTheSecondMaterialAtTheThreshold)
{
    const MixMaterial mix{.first = first_material,
                          .second = second_material,
                          .pattern = {.scale = 1, .veins = 3},
                          .threshold = 0.5};

    expectNear(reflectance(mix), second_color);
}

TEST(MixMaterialTest, thresholdZeroUsesOnlyTheSecondMaterial)
{
    const MixMaterial mix{.first = first_material,
                          .second = second_material,
                          .pattern = {.scale = 1},
                          .threshold = 0};

    expectNear(reflectance(mix), second_color);
}

TEST(MixMaterialTest, followsThePatternAcrossTheSurface)
{
    // Marble with scale 1 on the lattice weighs (1 + sin x) / 2: below one half
    // at x = -1 and above it at x = 1.
    const MixMaterial mix{.first = first_material,
                          .second = second_material,
                          .pattern = {.scale = 1, .veins = 3},
                          .threshold = 0.5};
    HitRecord hit{lattice_hit};

    hit.point = {-1.0, 2.0, -3.0};
    expectNear(reflectance(mix, hit), first_color);
    hit.point = {1.0, 2.0, -3.0};
    expectNear(reflectance(mix, hit), second_color);
}

TEST(MixMaterialTest, mixesMaterialsOfDifferentKinds)
{
    // A smooth conductor mirrors, so its sample from straight on comes back.
    const MixMaterial mix{.first = first_material,
                          .second = ConductorMaterial{.eta = {0.2, 0.4, 1.4}, .k = {3.6, 2.5, 1.9}},
                          .pattern = {.scale = 1},
                          .threshold = 0};

    const ScatterSample sample{
        requireValue(mix.scatteringAt(lattice_hit).sample(up, 0.5, {0.5, 0.5}))};

    expectNear(sample.direction, up);
}

Scattering scatteringWithThreshold(Real threshold)
{
    const MixMaterial mix{.first = first_material,
                          .second = second_material,
                          .pattern = {.scale = 1},
                          .threshold = threshold};
    return mix.scatteringAt(lattice_hit);
}

TEST(MixMaterialDeathTest, rejectsANegativeThreshold)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(scatteringWithThreshold(-0.1)), "");
}

TEST(MixMaterialDeathTest, rejectsAThresholdAboveOne)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(scatteringWithThreshold(1.1)), "");
}

TEST(MixMaterialDeathTest, rejectsANanThreshold)
{
    EXPECT_DEBUG_DEATH(
        static_cast<void>(scatteringWithThreshold(std::numeric_limits<Real>::quiet_NaN())), "");
}

} // namespace
} // namespace albedo
