#pragma once

#include "albedo/camera/pixel_sample.hpp"
#include "albedo/core/point3.hpp"
#include "albedo/core/ray.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"

namespace albedo {

/// How to place and shape a `PerspectiveCamera`.
struct CameraSettings {
    /// Camera position.
    Point3r look_from{};
    /// Point the camera looks at; it appears at the image center.
    Point3r look_at{};
    /// Which way is up. Need not be perpendicular to the view direction or unit length.
    Vector3r up{0.0, 1.0, 0.0};
    /// Angle between the top and bottom image edges, in degrees, in (0, 180).
    Real vertical_fov_degrees{60.0};
    /// Image width divided by height.
    Real aspect_ratio{16.0 / 9.0};
};

/// A pinhole camera: every ray starts at the camera position and passes
/// through a point on a virtual film in front of it.
class PerspectiveCamera {
public:
    /// Throws `std::invalid_argument` if the field of view is outside (0, 180)
    /// degrees, the aspect ratio is not a positive normal number, the position or
    /// target is not finite, the distance to the target or the length of `up` is
    /// zero, non-finite or outside roughly [1e-154, 1e154], `up` is (nearly)
    /// parallel to the view direction, or the film is so large that rays to its
    /// corners could not be normalized.
    explicit PerspectiveCamera(const CameraSettings& settings);

    /// The ray through `sample`'s film position, with unit-length direction.
    /// Precondition: `sample.film` lies in [0, 1]^2.
    [[nodiscard]] Ray generateRay(PixelSample sample) const noexcept;

private:
    // Camera position and the film's axes in world space, derived once from the settings.
    struct Frame {
        Point3r origin{};
        Vector3r forward{};
        Vector3r half_width{};
        Vector3r half_height{};
    };

    [[nodiscard]] static Frame buildFrame(const CameraSettings& settings);

    Frame frame;
};

} // namespace albedo
