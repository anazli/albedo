#pragma once

#include "albedo/color/rgb.hpp"
#include "albedo/scene/hit_record.hpp"
#include "albedo/texture/checker_texture.hpp"
#include "albedo/texture/constant_texture.hpp"
#include "albedo/texture/perlin_texture.hpp"

#include <concepts>
#include <variant>

namespace albedo {

/// What every texture alternative must provide.
template <typename T>
concept TextureLike = requires(const T& texture, const HitRecord& hit) {
    { texture.evaluate(hit) } -> std::same_as<Rgb>;
};

/// A variant whose alternatives must all satisfy `TextureLike`.
template <TextureLike... Textures>
using TextureVariant = std::variant<Textures...>;

/// Any texture the renderer supports: a color that varies over a surface. A
/// closed set, like `Material`. Default-constructs to black.
using Texture = TextureVariant<ConstantTexture, CheckerTexture, PerlinTexture>;

/// The color of `texture` where `hit` lies on a surface.
/// Not `noexcept`: `std::visit` throws `std::bad_variant_access` on a valueless variant.
[[nodiscard]] inline Rgb evaluate(const Texture& texture, const HitRecord& hit)
{
    return std::visit([&](const auto& alternative) { return alternative.evaluate(hit); }, texture);
}

} // namespace albedo
