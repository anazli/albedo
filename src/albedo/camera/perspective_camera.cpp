#include "albedo/camera/perspective_camera.hpp"

#include <cassert>
#include <cmath>
#include <format>
#include <stdexcept>

namespace albedo {

namespace {

// Below this, `up` is treated as parallel to the view direction: the right axis
// would be the normalized cross product of two nearly parallel unit vectors.
constexpr Real min_up_view_sine_squared{1e-12};

bool isFinite(Point3r p) noexcept
{
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}

void validateScalars(const CameraSettings& settings)
{
    const Real fov{settings.vertical_fov_degrees};
    // NaN fails both comparisons, so test it explicitly.
    if (std::isnan(fov) || fov <= 0 || fov >= 180) {
        throw std::invalid_argument{
            std::format("vertical field of view must be in (0, 180) degrees, got {}", fov)};
    }
    const Real aspect{settings.aspect_ratio};
    if (!(aspect > 0) || !std::isnormal(aspect)) {
        throw std::invalid_argument{
            std::format("aspect ratio must be a positive normal number, got {}", aspect)};
    }
    if (!isFinite(settings.look_from) || !isFinite(settings.look_at)) {
        throw std::invalid_argument{"camera position and target must be finite"};
    }
}

// Right-handed orthonormal camera basis: w points backwards, u right, v up.
struct Basis {
    Vector3r u{};
    Vector3r v{};
    Vector3r w{};
};

// Builds the basis from exactly the vectors it validates, so nothing the checks
// accept can make the construction lose precision.
Basis buildBasis(const CameraSettings& settings)
{
    const Vector3r backward{settings.look_from - settings.look_at};
    if (!std::isnormal(lengthSquared(backward))) {
        throw std::invalid_argument{"distance from camera position to target must be "
                                    "non-zero, finite and within roughly [1e-154, 1e154]"};
    }
    if (!std::isnormal(lengthSquared(settings.up))) {
        throw std::invalid_argument{"length of the camera up direction must be non-zero, "
                                    "finite and within roughly [1e-154, 1e154]"};
    }
    const Vector3r w{normalized(backward)};
    const Vector3r right{cross(normalized(settings.up), w)};
    if (lengthSquared(right) < min_up_view_sine_squared) {
        throw std::invalid_argument{"camera up direction must not be parallel to the view"};
    }
    const Vector3r u{normalized(right)};
    return {.u = u, .v = cross(w, u), .w = w};
}

} // namespace

PerspectiveCamera::PerspectiveCamera(const CameraSettings& settings) : frame{buildFrame(settings)}
{
}

Ray PerspectiveCamera::generateRay(PixelSample sample) const noexcept
{
    // The constructor only guarantees normalizable rays through [0, 1]^2; the
    // comparisons also reject NaN.
    assert(sample.film.x >= 0 && sample.film.x <= 1 && sample.film.y >= 0 && sample.film.y <= 1 &&
           "film position must lie in [0, 1]^2");
    // Film x grows to the right and y downwards; map both to [-1, 1] with +1 at
    // the right and the top edge.
    const Real horizontal{2 * sample.film.x - 1};
    const Real vertical{1 - 2 * sample.film.y};
    const Vector3r direction{frame.forward + frame.half_width * horizontal +
                             frame.half_height * vertical};
    return {.origin = frame.origin, .direction = normalized(direction)};
}

PerspectiveCamera::Frame PerspectiveCamera::buildFrame(const CameraSettings& settings)
{
    validateScalars(settings);
    const Basis basis{buildBasis(settings)};

    // The film sits at distance 1 in front of the camera, so its half extents
    // are the tangents of the half angles.
    const Real half_fov{settings.vertical_fov_degrees * pi / 360};
    const Real half_height{std::tan(half_fov)};
    const Real half_width{settings.aspect_ratio * half_height};
    const Frame result{.origin = settings.look_from,
                       .forward = -basis.w,
                       .half_width = basis.u * half_width,
                       .half_height = basis.v * half_height};

    // The ray to a film corner is the longest; if it can be normalized, every
    // ray through the film can.
    if (!std::isnormal(lengthSquared(result.forward + result.half_width + result.half_height))) {
        throw std::invalid_argument{"field of view and aspect ratio make the film too large"};
    }
    return result;
}

} // namespace albedo
