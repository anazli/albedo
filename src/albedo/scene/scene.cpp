#include "albedo/scene/scene.hpp"

#include "albedo/core/normal3.hpp"
#include "albedo/core/vector3.hpp"

#include <cassert>

namespace albedo {

void Scene::add(const Shape& shape, const Material& material)
{
    objects.push_back({.shape = shape, .material = material});
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

} // namespace albedo
