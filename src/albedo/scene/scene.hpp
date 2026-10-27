#pragma once

#include "albedo/accel/bvh.hpp"
#include "albedo/color/rgb.hpp"
#include "albedo/core/bounds3.hpp"
#include "albedo/core/interval.hpp"
#include "albedo/core/ray.hpp"
#include "albedo/geometry/shape.hpp"
#include "albedo/geometry/sphere.hpp"
#include "albedo/light/light.hpp"
#include "albedo/light/sky_light.hpp"
#include "albedo/material/material.hpp"
#include "albedo/scattering/scattering.hpp"
#include "albedo/scene/hit_record.hpp"
#include "albedo/scene/scene_object.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace albedo {

class TriangleMesh;

/// The objects a ray can hit, and the lights: point lights, emitting objects and
/// the sky around everything. Intersection searches a bounding volume hierarchy
/// that every call adding objects rebuilds in full, so a scene is always ready to
/// render; a large scene is best built with few calls (one per mesh, not one per
/// triangle). Every function that adds something adds nothing if it throws.
class Scene {
public:
    /// Adds an object with the given geometry and surface material.
    /// Throws only for lack of memory, or what `bounds(const Shape&)` throws.
    void add(const Shape& shape, const Material& material);

    /// Adds every triangle of `mesh` (see `TriangleMesh::triangles`) as an object
    /// made of `material`, in face order. Throws only for lack of memory.
    void add(const TriangleMesh& mesh, const Material& material);

    /// Number of objects in the scene.
    [[nodiscard]] std::size_t shapeCount() const noexcept { return objects.size(); }

    /// Adds a sphere that emits `radiance` from its outside and scatters light
    /// like `material`, as both an object and a light.
    void addAreaLight(const Sphere& sphere, Rgb radiance, const Material& material);

    /// Adds a point light and returns its index in `lights()`. Area lights and
    /// the sky have their own functions, which keep each linked to what a ray
    /// can hit.
    std::size_t addLight(const PointLight& light);

    /// Sets the light arriving from beyond the scene, replacing any earlier sky.
    /// The sky is black, and not among `lights()`, until set.
    void setSky(const SkyLight& new_sky);

    /// The light arriving from beyond the scene.
    [[nodiscard]] SkyLight sky() const noexcept;

    /// Every light, for sampling direct illumination.
    [[nodiscard]] std::span<const Light> lights() const noexcept { return light_list; }

    /// The nearest hit with `t` strictly inside `t_range`, if any. The record's
    /// normal faces against the ray. Throws `std::bad_variant_access` only if a
    /// stored shape is valueless (see `intersect(const Shape&, ...)`), or
    /// `std::out_of_range` if a hierarchy's depth bound is violated (see `Bvh`).
    [[nodiscard]] std::optional<HitRecord> intersect(const Ray& ray, Interval t_range) const;

    /// How light scatters at `hit`, from the material of the object hit.
    /// Precondition: `hit` came from this scene's `intersect`.
    /// Throws only what `scatteringAt(const Material&, ...)` throws.
    [[nodiscard]] Scattering scatteringAt(const HitRecord& hit) const;

    /// Radiance emitted towards the ray at `hit`: black unless the object hit is
    /// an area light seen from outside. Precondition: as for `scatteringAt`.
    [[nodiscard]] Rgb emittedRadiance(const HitRecord& hit) const noexcept;

    /// The index in `lights()` of the area light hit, if the object hit is one.
    /// Precondition: as for `scatteringAt`.
    [[nodiscard]] std::optional<std::size_t> lightIndexOf(const HitRecord& hit) const noexcept;

    /// True when any object lies along `ray` with `t` strictly inside `t_range`,
    /// as a shadow ray needs. Throws only what `intersect` throws.
    [[nodiscard]] bool occluded(const Ray& ray, Interval t_range) const;

private:
    // Appends to the light list and returns the new light's index.
    std::size_t appendLight(const Light& light);

    // A hierarchy over the current objects followed by objects with `added_bounds`,
    // built without changing the scene.
    [[nodiscard]] Bvh hierarchyWith(std::span<const Bounds3r> added_bounds) const;

    std::vector<SceneObject> objects;
    std::vector<Light> light_list;
    std::optional<std::size_t> sky_index;
    Bvh hierarchy;
};

} // namespace albedo
