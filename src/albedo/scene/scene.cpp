#include "albedo/scene/scene.hpp"

#include "albedo/core/normal3.hpp"
#include "albedo/core/vector3.hpp"
#include "albedo/geometry/triangle.hpp"
#include "albedo/geometry/triangle_mesh.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <span>
#include <type_traits>
#include <variant>
#include <vector>

namespace albedo {

namespace {

// Installing a new hierarchy must not fail once the scene has changed.
static_assert(std::is_nothrow_move_assignable_v<Bvh>);

} // namespace

void Scene::add(const Shape& shape, const Material& material)
{
    const std::array<Bounds3r, 1> added{bounds(shape)};
    Bvh next{hierarchyWith(added)};
    objects.push_back({.shape = shape, .material = material, .light = std::nullopt});
    hierarchy = std::move(next);
}

void Scene::add(const TriangleMesh& mesh, const Material& material)
{
    // Reserving up front, with appends that cannot throw, means the loop cannot
    // fail part-way. Doubling keeps repeated calls amortized, as in addAreaLight.
    // Each append builds an object from a triangle and the material, then moves
    // it into place. (Copying a SceneObject is not needed, and is not guaranteed
    // to be noexcept: the standard leaves std::variant's copy constructor without
    // a noexcept specification unless every alternative is trivially copy
    // constructible.)
    static_assert(std::is_nothrow_constructible_v<Shape, const Triangle&> &&
                      std::is_nothrow_copy_constructible_v<Material> &&
                      std::is_nothrow_move_constructible_v<SceneObject>,
                  "Scene::add(const TriangleMesh&) relies on non-throwing appends after reserve");
    const std::vector<Triangle> triangles{mesh.triangles()};
    std::vector<Bounds3r> added;
    added.reserve(triangles.size());
    for (const Triangle& triangle : triangles) {
        added.push_back(triangle.bounds());
    }
    Bvh next{hierarchyWith(added)};
    const std::size_t needed{objects.size() + triangles.size()};
    if (needed > objects.capacity()) {
        objects.reserve(std::max(needed, 2 * objects.capacity()));
    }
    for (const Triangle& triangle : triangles) {
        objects.push_back({.shape = triangle, .material = material, .light = std::nullopt});
    }
    hierarchy = std::move(next);
}

void Scene::addAreaLight(const Sphere& sphere, Rgb radiance, const Material& material)
{
    // Make room first so the object cannot fail to be added after its light was:
    // that would leave a light no ray can hit. Doubling keeps repeated calls
    // amortized constant, as push_back's own growth would.
    static_assert(std::is_nothrow_constructible_v<Shape, const Sphere&> &&
                      std::is_nothrow_copy_constructible_v<Material> &&
                      std::is_nothrow_move_constructible_v<SceneObject>,
                  "Scene::addAreaLight relies on a non-throwing append after reserve");
    if (objects.size() == objects.capacity()) {
        objects.reserve(std::max<std::size_t>(1, 2 * objects.capacity()));
    }
    const std::array<Bounds3r, 1> added{sphere.bounds()};
    Bvh next{hierarchyWith(added)};
    const std::size_t light{appendLight(AreaLight{.shape = sphere, .radiance = radiance})};
    objects.push_back({.shape = sphere, .material = material, .light = light});
    hierarchy = std::move(next);
}

std::size_t Scene::addLight(const PointLight& light)
{
    return appendLight(light);
}

void Scene::setSky(const SkyLight& new_sky)
{
    if (sky_index) {
        light_list[*sky_index] = new_sky;
    } else {
        sky_index = appendLight(new_sky);
    }
}

SkyLight Scene::sky() const noexcept
{
    if (!sky_index) {
        return {};
    }
    // get_if rather than std::get keeps the exception path out of a noexcept function.
    const auto* stored{std::get_if<SkyLight>(&light_list[*sky_index])};
    assert(stored != nullptr && "the sky index must refer to a SkyLight");
    return *stored;
}

std::optional<HitRecord> Scene::intersect(const Ray& ray, Interval t_range) const
{
    // The hierarchy shrinks the range to each hit, so the last hit kept is the nearest.
    std::optional<ShapeHit> nearest;
    const auto nearest_object{hierarchy.nearestHit(
        ray, t_range, [&](std::size_t index, Interval range) -> std::optional<Real> {
            const auto hit{albedo::intersect(objects[index].shape, ray, range)};
            if (!hit) {
                return std::nullopt;
            }
            nearest = hit;
            return hit->t;
        })};
    if (!nearest_object || !nearest) {
        return std::nullopt;
    }

    const bool front_face{dot(ray.direction, nearest->normal) < 0};
    return HitRecord{.t = nearest->t,
                     .point = nearest->point,
                     .normal = front_face ? nearest->normal : -nearest->normal,
                     .shading_normal =
                         front_face ? nearest->shading_normal : -nearest->shading_normal,
                     .front_face = front_face,
                     .uv = albedo::textureCoordinatesAt(objects[*nearest_object].shape, *nearest),
                     .object = *nearest_object};
}

Scattering Scene::scatteringAt(const HitRecord& hit) const
{
    assert(hit.object < objects.size() && "hit does not come from this scene");
    return albedo::scatteringAt(objects[hit.object].material, hit);
}

Rgb Scene::emittedRadiance(const HitRecord& hit) const noexcept
{
    const auto light{lightIndexOf(hit)};
    if (!light) {
        return {};
    }
    const auto* area{std::get_if<AreaLight>(&light_list[*light])};
    return area != nullptr ? area->emittedRadiance(hit) : Rgb{};
}

std::optional<std::size_t> Scene::lightIndexOf(const HitRecord& hit) const noexcept
{
    assert(hit.object < objects.size() && "hit does not come from this scene");
    return objects[hit.object].light;
}

bool Scene::occluded(const Ray& ray, Interval t_range) const
{
    // Any hit will do, so the search stops at the first one.
    return hierarchy.anyHit(ray, t_range,
                            [&](std::size_t index, Interval range) -> std::optional<Real> {
                                const auto hit{albedo::intersect(objects[index].shape, ray, range)};
                                return hit ? std::optional<Real>{hit->t} : std::nullopt;
                            });
}

std::size_t Scene::appendLight(const Light& light)
{
    light_list.push_back(light);
    return light_list.size() - 1;
}

Bvh Scene::hierarchyWith(std::span<const Bounds3r> added_bounds) const
{
    std::vector<Bounds3r> all_bounds;
    all_bounds.reserve(objects.size() + added_bounds.size());
    for (const SceneObject& object : objects) {
        all_bounds.push_back(bounds(object.shape));
    }
    all_bounds.insert(all_bounds.end(), added_bounds.begin(), added_bounds.end());
    return Bvh{all_bounds};
}

} // namespace albedo
