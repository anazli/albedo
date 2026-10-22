#pragma once

#include "albedo/color/rgb.hpp"
#include "albedo/core/ray.hpp"
#include "albedo/estimator/normals_estimator.hpp"
#include "albedo/estimator/path_estimator.hpp"
#include "albedo/estimator/scatter_only_estimator.hpp"
#include "albedo/sampling/sample_generator.hpp"
#include "albedo/scene/scene.hpp"

#include <concepts>
#include <variant>

namespace albedo {

/// What every estimator alternative must provide.
template <typename E>
concept EstimatorLike =
    requires(const E& estimator, const Ray& ray, const Scene& scene, SampleGenerator& generator) {
        { estimator.radiance(ray, scene, generator) } -> std::same_as<Rgb>;
    };

/// A variant whose alternatives must all satisfy `EstimatorLike`.
template <EstimatorLike... Estimators>
using RadianceEstimatorVariant = std::variant<Estimators...>;

/// Any way the renderer can estimate the radiance along a camera ray. A closed
/// set, like `Shape`.
using RadianceEstimator =
    RadianceEstimatorVariant<NormalsEstimator, ScatterOnlyEstimator, PathEstimator>;

/// One sample of the radiance arriving along `ray`, from the held estimator.
/// Not `noexcept`: `std::visit` throws `std::bad_variant_access` on a valueless
/// variant, and the estimators throw what `Scene` throws.
[[nodiscard]] inline Rgb radiance(const RadianceEstimator& estimator, const Ray& ray,
                                  const Scene& scene, SampleGenerator& generator)
{
    return std::visit(
        [&](const auto& alternative) { return alternative.radiance(ray, scene, generator); },
        estimator);
}

} // namespace albedo
