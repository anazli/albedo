#pragma once

#include "albedo/color/rgb.hpp"
#include "albedo/core/point2.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"
#include "albedo/scattering/conductor_lobe.hpp"
#include "albedo/scattering/dielectric_lobe.hpp"
#include "albedo/scattering/lambert_lobe.hpp"
#include "albedo/scattering/scatter_sample.hpp"

#include <concepts>
#include <optional>
#include <variant>

namespace albedo {

/// What every lobe alternative must provide, all in local space (normal = +z).
template <typename L>
concept LobeLike = requires(const L& lobe, Vector3r wo, Vector3r wi, Real u_choice, Point2r u) {
    { lobe.evaluate(wo, wi) } -> std::same_as<Rgb>;
    { lobe.sample(wo, u_choice, u) } -> std::same_as<std::optional<ScatterSample>>;
    { lobe.pdf(wo, wi) } -> std::same_as<Real>;
};

/// A variant whose alternatives must all satisfy `LobeLike`.
template <LobeLike... Lobes>
using LobeVariant = std::variant<Lobes...>;

/// Any scattering model the renderer supports. A closed set, like `Shape`.
using Lobe = LobeVariant<LambertLobe, ConductorLobe, DielectricLobe>;

/// The held lobe's f(wo, wi) in local space.
/// Not `noexcept`: `std::visit` throws `std::bad_variant_access` on a valueless variant.
[[nodiscard]] inline Rgb evaluate(const Lobe& lobe, Vector3r wo, Vector3r wi)
{
    return std::visit([&](const auto& alternative) { return alternative.evaluate(wo, wi); }, lobe);
}

/// A local-space sample of the held lobe, if any. `u_choice` in [0, 1) selects
/// between ways of scattering (such as reflection or refraction) for lobes that
/// have several; `u` in [0, 1]^2 places the direction.
/// Not `noexcept`: `std::visit` throws `std::bad_variant_access` on a valueless variant.
[[nodiscard]] inline std::optional<ScatterSample> sample(const Lobe& lobe, Vector3r wo,
                                                         Real u_choice, Point2r u)
{
    return std::visit([&](const auto& alternative) { return alternative.sample(wo, u_choice, u); },
                      lobe);
}

/// The held lobe's sampling density of `wi` given `wo`, in local space.
/// Not `noexcept`: `std::visit` throws `std::bad_variant_access` on a valueless variant.
[[nodiscard]] inline Real pdf(const Lobe& lobe, Vector3r wo, Vector3r wi)
{
    return std::visit([&](const auto& alternative) { return alternative.pdf(wo, wi); }, lobe);
}

} // namespace albedo
