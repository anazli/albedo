#pragma once

#include "albedo/core/point2.hpp"
#include "albedo/core/point3.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"
#include "albedo/light/area_light.hpp"
#include "albedo/light/light_sample.hpp"
#include "albedo/light/point_light.hpp"
#include "albedo/light/sky_light.hpp"

#include <concepts>
#include <optional>
#include <variant>

namespace albedo {

/// What every light alternative must provide for direct lighting.
template <typename L>
concept LightLike = requires(const L& light, Point3r point, Point2r u, Vector3r direction) {
    { light.sampleIncidentLight(point, u) } -> std::same_as<std::optional<LightSample>>;
    { light.incidentLightPdf(point, direction) } -> std::same_as<Real>;
};

/// A variant whose alternatives must all satisfy `LightLike`.
template <LightLike... Lights>
using LightVariant = std::variant<Lights...>;

/// Any light the renderer supports. A closed set, like `Shape`.
using Light = LightVariant<PointLight, AreaLight, SkyLight>;

/// A direction from `point` towards the held light, if any.
/// Not `noexcept`: `std::visit` throws `std::bad_variant_access` on a valueless variant.
[[nodiscard]] inline std::optional<LightSample> sampleIncidentLight(const Light& light,
                                                                    Point3r point, Point2r u)
{
    return std::visit(
        [&](const auto& alternative) { return alternative.sampleIncidentLight(point, u); }, light);
}

/// Density with which the held light's sampling picks `direction` from `point`.
/// Not `noexcept`: `std::visit` throws `std::bad_variant_access` on a valueless variant.
[[nodiscard]] inline Real incidentLightPdf(const Light& light, Point3r point, Vector3r direction)
{
    return std::visit(
        [&](const auto& alternative) { return alternative.incidentLightPdf(point, direction); },
        light);
}

} // namespace albedo
