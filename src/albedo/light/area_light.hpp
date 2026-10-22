#pragma once

#include "albedo/color/rgb.hpp"
#include "albedo/core/point2.hpp"
#include "albedo/core/point3.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"
#include "albedo/geometry/sphere.hpp"
#include "albedo/light/light_sample.hpp"
#include "albedo/scene/hit_record.hpp"

#include <optional>

namespace albedo {

/// A sphere whose outside surface emits the same radiance everywhere and in
/// every direction. Spheres are the only shape so far; other shapes come with
/// triangle meshes. Precondition: `radiance` is non-negative per channel.
struct AreaLight {
    /// The emitting surface.
    Sphere shape;
    /// Radiance leaving the surface, per channel.
    Rgb radiance{};

    /// A direction from `point` towards the sphere for the random point `u` in
    /// [0, 1]^2, uniform over the cone of directions the sphere covers (Shirley
    /// et al., "Monte Carlo Techniques for Direct Lighting Calculations", ACM TOG
    /// 15(1), 1996). Nothing from inside the sphere, whose inside does not emit,
    /// or when the sphere is too small to resolve from `point`.
    [[nodiscard]] std::optional<LightSample> sampleIncidentLight(Point3r point,
                                                                 Point2r u) const noexcept;

    /// Density with which `sampleIncidentLight` picks `direction`: one over the
    /// covered solid angle inside the cone; zero outside it, and everywhere when
    /// `sampleIncidentLight` gives nothing. Precondition: `direction` has unit length.
    [[nodiscard]] Real incidentLightPdf(Point3r point, Vector3r direction) const noexcept;

    /// Radiance a ray sees where it hits the sphere: the emitted radiance on the
    /// outside, black on the inside.
    [[nodiscard]] Rgb emittedRadiance(const HitRecord& hit) const noexcept
    {
        return hit.front_face ? radiance : Rgb{};
    }
};

} // namespace albedo
