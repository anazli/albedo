#pragma once

#include "albedo/core/interval.hpp"
#include "albedo/core/ray.hpp"
#include "albedo/geometry/shape.hpp"
#include "albedo/light/sky_light.hpp"
#include "albedo/material/material.hpp"
#include "albedo/scattering/scattering.hpp"
#include "albedo/scene/hit_record.hpp"
#include "albedo/scene/scene_object.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace albedo {

/// The objects a ray can hit and the sky around them. Intersection tests every
/// object; an acceleration structure replaces the linear search in a later step.
class Scene {
public:
    /// Adds an object with the given geometry and surface material.
    void add(const Shape& shape, const Material& material);

    /// Number of objects in the scene.
    [[nodiscard]] std::size_t shapeCount() const noexcept { return objects.size(); }

    /// Replaces the light arriving from beyond the scene; black until set.
    void setSky(const SkyLight& new_sky) noexcept { sky_light = new_sky; }

    /// The light arriving from beyond the scene.
    [[nodiscard]] const SkyLight& sky() const noexcept { return sky_light; }

    /// The nearest hit with `t` strictly inside `t_range`, if any. The record's
    /// normal faces against the ray. Throws `std::bad_variant_access` only if a
    /// stored shape is valueless (see `intersect(const Shape&, ...)`).
    [[nodiscard]] std::optional<HitRecord> intersect(const Ray& ray, Interval t_range) const;

    /// How light scatters at `hit`, from the material of the object hit.
    /// Precondition: `hit` came from this scene's `intersect`.
    /// Throws only what `scatteringAt(const Material&, ...)` throws.
    [[nodiscard]] Scattering scatteringAt(const HitRecord& hit) const;

private:
    std::vector<SceneObject> objects;
    SkyLight sky_light;
};

} // namespace albedo
