#include "albedo/scene/scene.hpp"

#include "albedo/core/normal3.hpp"
#include "albedo/core/vector3.hpp"

namespace albedo {

void Scene::add(const Shape& shape)
{
    shapes.push_back(shape);
}

std::optional<HitRecord> Scene::intersect(const Ray& ray, Interval t_range) const
{
    std::optional<ShapeHit> nearest;
    Interval remaining{t_range};
    for (const Shape& shape : shapes) {
        if (const auto hit{albedo::intersect(shape, ray, remaining)}) {
            nearest = hit;
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
                     .front_face = front_face};
}

} // namespace albedo
