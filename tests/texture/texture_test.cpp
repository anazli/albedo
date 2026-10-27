#include "albedo/texture/texture.hpp"

#include "support/expect_near.hpp"

#include <gtest/gtest.h>

namespace albedo {
namespace {

using test_support::exact_tolerance;
using test_support::expectNear;

// Has no `evaluate`, so it must not count as a texture.
struct TextureWithoutEvaluate {};

// Has an `evaluate` that does not return a color, so it must not count either.
struct TextureEvaluatingToNumber {
    [[nodiscard]] static int evaluate(const HitRecord& /*hit*/) noexcept { return 0; }
};

template <typename... Textures>
concept CanFormTextureVariant = requires { typename TextureVariant<Textures...>; };

TEST(TextureTest, typeWithoutEvaluateIsNotATexture)
{
    static_assert(TextureLike<ConstantTexture>);
    static_assert(TextureLike<CheckerTexture>);
    static_assert(!TextureLike<TextureWithoutEvaluate>);
    static_assert(!TextureLike<TextureEvaluatingToNumber>);
}

TEST(TextureTest, textureVariantRejectsNonTextureAlternatives)
{
    static_assert(CanFormTextureVariant<ConstantTexture>);
    static_assert(!CanFormTextureVariant<ConstantTexture, TextureWithoutEvaluate>);
}

TEST(TextureTest, evaluateDispatchesToTheAlternativeHeld)
{
    constexpr Rgb odd_color{0.1, 0.2, 0.3};
    const Texture texture{CheckerTexture{.even = {}, .odd = odd_color, .frequency = 1.0}};

    expectNear(evaluate(texture, HitRecord{.uv = {1.5, 0.5}}), odd_color, exact_tolerance);
}

TEST(TextureTest, defaultTextureIsBlack)
{
    EXPECT_TRUE(isBlack(evaluate(Texture{}, HitRecord{})));
}

} // namespace
} // namespace albedo
