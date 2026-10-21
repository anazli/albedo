#pragma once

#include "albedo/color/rgb.hpp"
#include "albedo/core/ray.hpp"
#include "albedo/scene/scene.hpp"

namespace albedo {

/// A debugging view rather than physical light: colors each hit by its surface
/// normal (each axis from [-1, 1] mapped to a channel in [0, 1]), and rays that
/// miss everything by a white-to-blue sky gradient.
struct NormalsEstimator {
    /// The color seen along `ray`. Precondition: `ray.direction` can be normalized.
    /// Throws only what `Scene::intersect` throws.
    [[nodiscard]] static Rgb radiance(const Ray& ray, const Scene& scene);
};

} // namespace albedo
