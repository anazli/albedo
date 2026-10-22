#include "albedo/scattering/fresnel.hpp"

#include "support/expect_near.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::expectNear;

constexpr Real glass{1.5};

// The Fresnel equations in their angle form (Hecht, "Optics", sec. 4.6), an
// independent route to the same reflectance for a dielectric boundary.
Real reflectanceFromAngles(Real theta_i, Real eta)
{
    const Real theta_t{std::asin(std::sin(theta_i) / eta)};
    const Real r_perpendicular{-std::sin(theta_i - theta_t) / std::sin(theta_i + theta_t)};
    const Real r_parallel{std::tan(theta_i - theta_t) / std::tan(theta_i + theta_t)};
    return (r_perpendicular * r_perpendicular + r_parallel * r_parallel) / 2;
}

TEST(FresnelTest, dielectricAtNormalIncidenceIsTheSquaredIndexRatio)
{
    EXPECT_NEAR(fresnelDielectric(1.0, glass), 0.04, default_tolerance);
}

TEST(FresnelTest, dielectricMatchesTheAngleForm)
{
    for (const Real theta_i : {0.2, 0.7, 1.2, 1.5}) {
        SCOPED_TRACE(::testing::Message() << "theta " << theta_i);
        EXPECT_NEAR(fresnelDielectric(std::cos(theta_i), glass),
                    reflectanceFromAngles(theta_i, glass), 1e-12);
    }
}

TEST(FresnelTest, dielectricReflectsOnlyPerpendicularLightAtBrewstersAngle)
{
    const Real brewster{std::atan(glass)};
    const Real theta_t{pi / 2 - brewster};
    const Real r_perpendicular{std::sin(brewster - theta_t) / std::sin(brewster + theta_t)};

    EXPECT_NEAR(fresnelDielectric(std::cos(brewster), glass), r_perpendicular * r_perpendicular / 2,
                1e-12);
}

TEST(FresnelTest, dielectricReflectsEverythingAtGrazing)
{
    EXPECT_NEAR(fresnelDielectric(0.0, glass), 1.0, default_tolerance);
}

TEST(FresnelTest, matchedIndicesReflectNothing)
{
    EXPECT_NEAR(fresnelDielectric(0.6, 1.0), 0.0, default_tolerance);
}

TEST(FresnelTest, matchedIndicesReflectNothingEvenAtGrazing)
{
    // Grazing incidence must not be mistaken for the critical angle.
    EXPECT_NEAR(fresnelDielectric(0.0, 1.0), 0.0, default_tolerance);
}

TEST(FresnelTest, dielectricReflectsTotallyBeyondTheCriticalAngle)
{
    // Leaving glass: the critical angle has sin = 1 / 1.5, about 41.8 degrees.
    EXPECT_NEAR(fresnelDielectric(std::cos(0.8), 1 / glass), 1.0, default_tolerance);
}

TEST(FresnelTest, dielectricFromBelowUsesTheInvertedRatio)
{
    EXPECT_NEAR(fresnelDielectric(-0.6, glass), fresnelDielectric(0.6, 1 / glass),
                default_tolerance);
}

TEST(FresnelTest, conductorAtNormalIncidenceMatchesTheClosedForm)
{
    // ((n - 1)^2 + k^2) / ((n + 1)^2 + k^2) per channel.
    constexpr Rgb eta{0.2, 0.9, 1.4};
    constexpr Rgb k{3.0, 2.4, 1.6};
    const auto closed_form{[](Real n, Real kappa) {
        return ((n - 1) * (n - 1) + kappa * kappa) / ((n + 1) * (n + 1) + kappa * kappa);
    }};

    expectNear(fresnelConductor(1.0, eta, k),
               Rgb{closed_form(eta.r, k.r), closed_form(eta.g, k.g), closed_form(eta.b, k.b)});
}

TEST(FresnelTest, conductorWithoutAbsorptionIsADielectric)
{
    for (const Real cos_theta_i : {1.0, 0.8, 0.4, 0.1}) {
        SCOPED_TRACE(::testing::Message() << "cos theta " << cos_theta_i);
        const Real dielectric{fresnelDielectric(cos_theta_i, glass)};
        expectNear(fresnelConductor(cos_theta_i, {glass, glass, glass}, {}),
                   Rgb{dielectric, dielectric, dielectric}, 1e-12);
    }
}

TEST(FresnelTest, conductorClampsCosinesThatRoundingPushedOutOfRange)
{
    constexpr Rgb eta{0.2, 0.9, 1.4};
    constexpr Rgb k{3.0, 2.4, 1.6};

    expectNear(fresnelConductor(1 + 1e-15, eta, k), fresnelConductor(1.0, eta, k));
    expectNear(fresnelConductor(-1e-15, eta, k), fresnelConductor(0.0, eta, k));
}

TEST(FresnelTest, conductorReflectsEverythingAtGrazing)
{
    expectNear(fresnelConductor(0.0, {0.2, 0.9, 1.4}, {3.0, 2.4, 1.6}), Rgb{1.0, 1.0, 1.0}, 1e-12);
}

TEST(FresnelTest, conductorReflectanceStaysInTheUnitInterval)
{
    for (int i{0}; i <= 100; ++i) {
        const Rgb reflectance{fresnelConductor(i / 100.0, {0.2, 0.9, 1.4}, {3.0, 2.4, 1.6})};
        for (const Real channel : {reflectance.r, reflectance.g, reflectance.b}) {
            ASSERT_GE(channel, 0.0);
            ASSERT_LE(channel, 1.0 + 1e-12);
        }
    }
}

TEST(FresnelDeathTest, dielectricRejectsNonPositiveIndexRatio)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(fresnelDielectric(0.5, 0.0)), "");
}

TEST(FresnelDeathTest, conductorRejectsNonPositiveIndex)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(fresnelConductor(0.5, {1.0, 0.0, 1.0}, {1.0, 1.0, 1.0})),
                       "");
}

TEST(FresnelDeathTest, conductorRejectsNegativeAbsorption)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(fresnelConductor(0.5, {1.0, 1.0, 1.0}, {1.0, -1.0, 1.0})),
                       "");
}

} // namespace
} // namespace albedo
