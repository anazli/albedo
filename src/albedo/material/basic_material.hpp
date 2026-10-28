#pragma once

#include "albedo/material/conductor_material.hpp"
#include "albedo/material/dielectric_material.hpp"
#include "albedo/material/diffuse_material.hpp"
#include "albedo/scattering/scattering.hpp"
#include "albedo/scene/hit_record.hpp"

#include <concepts>
#include <variant>

namespace albedo {

/// What every material alternative must provide.
template <typename M>
concept MaterialLike = requires(const M& material, const HitRecord& hit) {
    { material.scatteringAt(hit) } -> std::same_as<Scattering>;
};

/// A variant whose alternatives must all satisfy `MaterialLike`.
template <MaterialLike... Materials>
using MaterialVariant = std::variant<Materials...>;

/// A material that is not made of other materials: what `MixMaterial` mixes.
/// Keeping mixes out of it keeps every material a plain value, with no
/// recursion through the variant.
using BasicMaterial = MaterialVariant<DiffuseMaterial, ConductorMaterial, DielectricMaterial>;

/// How light scatters where `hit` lies on a surface made of `material`, for any
/// material variant. Not `noexcept`: `std::visit` throws
/// `std::bad_variant_access` on a valueless variant.
template <MaterialLike... Materials>
[[nodiscard]] Scattering scatteringAt(const MaterialVariant<Materials...>& material,
                                      const HitRecord& hit)
{
    return std::visit([&](const auto& alternative) { return alternative.scatteringAt(hit); },
                      material);
}

} // namespace albedo
