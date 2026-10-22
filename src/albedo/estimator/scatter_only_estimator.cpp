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

// The ray leaving `hit` in `direction`, which lies on the side the normal faces.
Ray continuedRay(const HitRecord& hit, Vector3r direction) noexcept
{
    return {.origin = hit.point + asVector(hit.normal) * surface_offset, .direction = direction};
}

} // namespace

Rgb ScatterOnlyEstimator::radiance(const Ray& ray, const Scene& scene,
                                   SampleGenerator& generator) const
{
    assert(max_scatter_count >= 0 && "scatter count limit must not be negative");
    Ray path_ray{ray};
    // Product of the sample weights so far: how much of the light found at the
    // end of the path reaches the camera.
    Rgb throughput{1, 1, 1};
    for (int scatter_count{0};; ++scatter_count) {
        const auto hit{scene.intersect(path_ray, in_front_of_origin)};
        if (!hit) {
            return throughput * scene.sky().emittedRadiance(path_ray.direction);
        }
        // `>=` rather than `==`, so a negative limit in a release build (where the
        // assert is gone) ends the path instead of looping forever.
        if (scatter_count >= max_scatter_count) {
            return {};
        }
        const auto sample{scene.scatteringAt(*hit).sample(-path_ray.direction, generator.next2D())};
        if (!sample) {
            return {};
        }
        throughput *= sample->weight;
        path_ray = continuedRay(*hit, sample->direction);
    }
}

} // namespace albedo
