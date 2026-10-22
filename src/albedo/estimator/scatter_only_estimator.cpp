#include "albedo/estimator/scatter_only_estimator.hpp"

#include "albedo/core/interval.hpp"
#include "albedo/core/normal3.hpp"
#include "albedo/core/point3.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"

#include <cassert>
#include <limits>

namespace albedo {

namespace {

constexpr Interval in_front_of_origin{.min = 0, .max = std::numeric_limits<Real>::infinity()};

// A continuing ray starts this far off the surface along the normal, so rounding
// error in the hit point cannot make it hit the same surface again at t ~ 0. It
// is far above that error for scenes of roughly unit to thousand-unit scale, and
// far below any feature size that matters there.
constexpr Real surface_offset{1e-7};

// The ray leaving `hit` in `direction`, started off the surface on the side the
// direction leaves: in front for reflection, behind for refraction.
Ray continuedRay(const HitRecord& hit, Vector3r direction) noexcept
{
    const Real side{dot(direction, hit.normal) >= 0 ? Real{1} : Real{-1}};
    return {.origin = hit.point + asVector(hit.normal) * (side * surface_offset),
            .direction = direction};
}

} // namespace

Rgb ScatterOnlyEstimator::radiance(const Ray& ray, const Scene& scene,
                                   SampleGenerator& generator) const
{
    assert(max_scatter_count >= 0 && "scatter count limit must not be negative");
    Ray path_ray{ray};
    // Product of the sample weights so far: how much of the light found further
    // along the path reaches the camera.
    Rgb throughput{1, 1, 1};
    Rgb result{};
    for (int scatter_count{0};; ++scatter_count) {
        const auto hit{scene.intersect(path_ray, in_front_of_origin)};
        if (!hit) {
            return result + throughput * scene.sky().emittedRadiance(path_ray.direction);
        }
        // Emitters seen along the path add their light before it scatters on.
        result += throughput * scene.emittedRadiance(*hit);
        // `>=` rather than `==`, so a negative limit in a release build (where the
        // assert is gone) ends the path instead of looping forever.
        if (scatter_count >= max_scatter_count) {
            return result;
        }
        const Real u_choice{generator.next1D()};
        const auto sample{
            scene.scatteringAt(*hit).sample(-path_ray.direction, u_choice, generator.next2D())};
        if (!sample) {
            return result;
        }
        throughput *= sample->weight;
        // A black surface ends the path: nothing further can reach the camera.
        if (isBlack(throughput)) {
            return result;
        }
        path_ray = continuedRay(*hit, sample->direction);
    }
}

} // namespace albedo
