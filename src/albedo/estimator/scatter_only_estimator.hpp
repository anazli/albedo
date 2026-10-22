#pragma once

#include "albedo/color/rgb.hpp"
#include "albedo/core/ray.hpp"
#include "albedo/sampling/sample_generator.hpp"
#include "albedo/scene/scene.hpp"

namespace albedo {

/// Estimates radiance by following one path: at each surface it samples the
/// material's scattering and continues, until the path escapes to the sky.
/// Surfaces do not emit and no light is sampled directly, so the sky is the only
/// source; next-event estimation arrives with `PathEstimator`.
struct ScatterOnlyEstimator {
    /// Most surface scatterings a path may have. A path still in the scene after
    /// that many returns black, which darkens deep interreflections slightly
    /// (Russian roulette removes this bias later). Must not be negative.
    int max_scatter_count{32};

    /// One sample of the radiance arriving along `ray`, drawing random numbers
    /// from `generator`. Precondition: `ray.direction` has unit length.
    /// Throws only what `Scene::intersect` and `Scene::scatteringAt` throw.
    [[nodiscard]] Rgb radiance(const Ray& ray, const Scene& scene,
                               SampleGenerator& generator) const;
};

} // namespace albedo
