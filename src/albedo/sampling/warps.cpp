#include "albedo/sampling/warps.hpp"

#include <algorithm>
#include <cmath>

namespace albedo {

Point2r sampleConcentricDisk(Point2r u) noexcept
{
    // Shirley and Chiu, "A Low Distortion Map Between Disk and Square" (1997):
    // concentric squares map to concentric circles, so strata stay compact.
    const Real offset_x{2 * u.x - 1};
    const Real offset_y{2 * u.y - 1};
    if (offset_x == 0 && offset_y == 0) {
        return {.x = 0, .y = 0};
    }
    Real radius{};
    Real angle{};
    if (std::abs(offset_x) > std::abs(offset_y)) {
        radius = offset_x;
        angle = (pi / 4) * (offset_y / offset_x);
    } else {
        radius = offset_y;
        angle = (pi / 2) - (pi / 4) * (offset_x / offset_y);
    }
    return {.x = radius * std::cos(angle), .y = radius * std::sin(angle)};
}

Vector3r sampleCosineHemisphere(Point2r u) noexcept
{
    // Malley's method: uniform points on the disk, projected up onto the
    // hemisphere, are distributed with density cos(theta) / pi.
    const Point2r disk{sampleConcentricDisk(u)};
    const Real z{std::sqrt(std::max(Real{0}, 1 - disk.x * disk.x - disk.y * disk.y))};
    return {disk.x, disk.y, z};
}

Vector3r sampleUniformSphere(Point2r u) noexcept
{
    // Archimedes' hat-box theorem: z uniform in [-1, 1] gives uniform area.
    const Real z{1 - 2 * u.x};
    const Real radius{std::sqrt(std::max(Real{0}, 1 - z * z))};
    const Real phi{2 * pi * u.y};
    return {radius * std::cos(phi), radius * std::sin(phi), z};
}

} // namespace albedo
