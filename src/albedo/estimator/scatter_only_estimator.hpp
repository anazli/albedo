#pragma once

#include "albedo/color/rgb.hpp"
#include "albedo/core/ray.hpp"
#include "albedo/sampling/sample_generator.hpp"
#include "albedo/scene/scene.hpp"

namespace albedo {

/// Estimates radiance by following one path: at each surface it samples the
/// material's scattering and continues, until the path escapes to the sky, is
/// absorbed (a sample fails or the throughput turns black) or reaches the limit.
/// Light is only found by hitting it: area lights along the path and the sky at
/// its end. Point lights are never hit, so they stay dark here. No light is
/// sampled directly; that is `PathEstimator`'s job. Slow to converge, but simple
/// enough to serve as its reference.
struct ScatterOnlyEstimator {
    /// Most surface scatterings a path may have. A path that reaches the limit
    /// returns the light gathered so far and drops what lies further along it,
    /// which darkens deep interreflections slightly. Must not be negative.
    int max_scatter_count{32};

    /// One sample of the radiance arriving along `ray`, drawing random numbers
    /// from `generator`. Precondition: `ray.direction` has unit length.
    /// Throws only what `Scene::intersect` and `Scene::scatteringAt` throw.
    [[nodiscard]] Rgb radiance(const Ray& ray, const Scene& scene,
                               SampleGenerator& generator) const;
};

} // namespace albedo
