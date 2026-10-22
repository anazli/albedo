#include "albedo/estimator/scatter_only_estimator.hpp"

#include "albedo/core/interval.hpp"
#include "albedo/core/normal3.hpp"
#include "albedo/core/point3.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"
#include "albedo/estimator/surface_ray.hpp"

#include <cassert>

namespace albedo {

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
        path_ray = rayLeaving(*hit, sample->direction);
    }
}

} // namespace albedo
