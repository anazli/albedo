#include "albedo/texture/constant_texture.hpp"

#include "support/expect_near.hpp"

#include <gtest/gtest.h>

namespace albedo {
namespace {

using test_support::exact_tolerance;
using test_support::expectNear;

TEST(ConstantTextureTest, isTheSameEverywhere)
{
    constexpr Rgb color{0.3, 0.6, 0.9};
    constexpr ConstantTexture texture{.color = color};

    expectNear(texture.evaluate(HitRecord{.uv = {0.0, 0.0}}), color, exact_tolerance);
    expectNear(texture.evaluate(HitRecord{.uv = {7.5, -3.25}}), color, exact_tolerance);
}

TEST(ConstantTextureTest, isBlackByDefault)
{
    EXPECT_TRUE(isBlack(ConstantTexture{}.evaluate(HitRecord{})));
}

} // namespace
} // namespace albedo
