#include "albedo/scene/scene.hpp"

#include "albedo/core/normal3.hpp"
#include "albedo/core/vector3.hpp"
#include "albedo/geometry/triangle.hpp"
#include "albedo/geometry/triangle_mesh.hpp"

#include <algorithm>
#include <cassert>
#include <type_traits>
#include <variant>
#include <vector>

namespace albedo {

void Scene::add(const Shape& shape, const Material& material)
{
    objects.push_back({.shape = shape, .material = material, .light = std::nullopt});
}

void Scene::add(const TriangleMesh& mesh, const Material& material)
{
    // Reserving up front, with appends that cannot throw, means the loop cannot
    // fail part-way. Doubling keeps repeated calls amortized, as in addAreaLight.
    static_assert(std::is_nothrow_copy_constructible_v<SceneObject> &&
                      std::is_nothrow_move_constructible_v<SceneObject>,
                  "Scene::add(const TriangleMesh&) relies on non-throwing appends after reserve");
    const std::vector<Triangle> triangles{mesh.triangles()};
    const std::size_t needed{objects.size() + triangles.size()};
    if (needed > objects.capacity()) {
        objects.reserve(std::max(needed, 2 * objects.capacity()));
    }
    for (const Triangle& triangle : triangles) {
        objects.push_back({.shape = triangle, .material = material, .light = std::nullopt});
    }
}

void Scene::addAreaLight(const Sphere& sphere, Rgb radiance, const Material& material)
{
    // Make room first so the object cannot fail to be added after its light was:
    // that would leave a light no ray can hit. Doubling keeps repeated calls
    // amortized constant, as push_back's own growth would.
    if (objects.size() == objects.capacity()) {
        objects.reserve(std::max<std::size_t>(1, 2 * objects.capacity()));
    }
    const std::size_t light{appendLight(AreaLight{.shape = sphere, .radiance = radiance})};
    objects.push_back({.shape = sphere, .material = material, .light = light});
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
    std::optional<ShapeHit> nearest;
    std::size_t nearest_object{};
    Interval remaining{t_range};
    for (std::size_t index{0}; index < objects.size(); ++index) {
        if (const auto hit{albedo::intersect(objects[index].shape, ray, remaining)}) {
            nearest = hit;
            nearest_object = index;
            // Only hits closer than this one can still matter.
            remaining.max = hit->t;
        }
    }
    if (!nearest) {
        return std::nullopt;
    }

    const bool front_face{dot(ray.direction, nearest->normal) < 0};
    return HitRecord{.t = nearest->t,
                     .point = nearest->point,
                     .normal = front_face ? nearest->normal : -nearest->normal,
                     .front_face = front_face,
                     .object = nearest_object};
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
    return std::ranges::any_of(objects, [&](const SceneObject& object) {
        return albedo::intersect(object.shape, ray, t_range).has_value();
    });
}

std::size_t Scene::appendLight(const Light& light)
{
    light_list.push_back(light);
    return light_list.size() - 1;
}

} // namespace albedo
