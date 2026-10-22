#pragma once

#include "albedo/color/rgb.hpp"
#include "albedo/core/ray.hpp"
#include "albedo/core/real.hpp"
#include "albedo/sampling/sample_generator.hpp"
#include "albedo/scene/scene.hpp"

namespace albedo {

/// Estimates radiance by unidirectional path tracing with next-event estimation
/// (Veach, "Robust Monte Carlo Methods for Light Transport Simulation", PhD
/// thesis, Stanford, 1997).
///
/// At every surface the path samples one light, chosen uniformly, and casts a
/// shadow ray towards it, then samples the material to continue. Light reached
/// both ways is combined with multiple importance sampling and the power
/// heuristic (Veach, sec. 9.2), so nothing is counted twice. Light only one
/// technique can find (point lights, or emitters seen through a mirror or
/// smooth glass) counts in full. After `roulette_start` bounces, Russian
/// roulette ends paths unlikely to matter without biasing the estimate.
struct PathEstimator {
    /// Most surface scatterings a path may have; a backstop, as roulette ends
    /// paths long before it in practice. Must not be negative.
    int max_depth{64};
    /// Scatterings before Russian roulette starts. Must not be negative.
    int roulette_start{3};

    /// Upper bound on the probability of surviving a roulette round. Below 1 so
    /// that paths through white, lossless scenes also end eventually.
    static constexpr Real max_survival_probability{0.95};

    /// One sample of the radiance arriving along `ray`, drawing random numbers
    /// from `generator`. Precondition: `ray.direction` has unit length.
    /// Throws only `std::bad_variant_access`, from a valueless variant.
    [[nodiscard]] Rgb radiance(const Ray& ray, const Scene& scene,
                               SampleGenerator& generator) const;
};

} // namespace albedo
