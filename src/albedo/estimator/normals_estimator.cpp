#include "albedo/estimator/normals_estimator.hpp"

#include "albedo/core/interval.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"

#include <limits>

namespace albedo {

namespace {

constexpr Interval in_front_of_origin{.min = 0, .max = std::numeric_limits<Real>::infinity()};
constexpr Rgb horizon_color{1.0, 1.0, 1.0};
constexpr Rgb zenith_color{0.5, 0.7, 1.0};

// Blends linearly with the direction's height: white straight down, blue
// straight up, halfway between at the horizon.
Rgb skyColor(Vector3r direction) noexcept
{
    const Real height{0.5 * (normalized(direction).y + 1)};
    return horizon_color * (1 - height) + zenith_color * height;
}

Rgb normalColor(Normal3r normal) noexcept
{
    return {.r = 0.5 * (normal.x + 1), .g = 0.5 * (normal.y + 1), .b = 0.5 * (normal.z + 1)};
}

} // namespace

Rgb NormalsEstimator::radiance(const Ray& ray, const Scene& scene)
{
    if (const auto hit{scene.intersect(ray, in_front_of_origin)}) {
        return normalColor(hit->shading_normal);
    }
    return skyColor(ray.direction);
}

} // namespace albedo
