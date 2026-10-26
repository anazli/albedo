#pragma once

#include "albedo/core/bounds3.hpp"
#include "albedo/core/interval.hpp"
#include "albedo/core/point3.hpp"
#include "albedo/core/ray.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"

#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace albedo {

/// Tests one primitive against a ray: called with the primitive's index and the
/// range of `t` still worth searching, it returns the `t` of the nearest hit
/// strictly inside that range, or nothing.
template <typename F>
concept PrimitiveIntersector =
    std::invocable<F&, std::size_t, Interval> &&
    std::same_as<std::invoke_result_t<F&, std::size_t, Interval>, std::optional<Real>>;

namespace detail {

/// One node of a `Bvh`, stored depth-first: an interior node's first child
/// follows it directly.
struct BvhNode {
    /// Box enclosing every primitive below the node.
    Bounds3r bounds{};
    /// Leaf: position of its first primitive in the primitive order.
    /// Interior: index of its second child.
    std::size_t offset{};
    /// Number of primitives in a leaf; zero marks an interior node.
    std::uint32_t count{};
    /// Interior: the axis (0, 1, 2 for x, y, z) its children were split along.
    std::uint8_t axis{};
};

/// Deepest a `Bvh` can be; see `Bvh::max_depth`.
inline constexpr std::size_t bvh_max_depth{128};

/// The ray along one axis, as the slab test needs it.
struct AxisRay {
    Real origin{};
    Real inverse_direction{};
};

/// The ray as traversal needs it, computed once per query.
struct TraversalRay {
    AxisRay x{};
    AxisRay y{};
    AxisRay z{};
    /// Bit `axis` is set when the ray runs towards smaller coordinates along that
    /// axis, so the second child of a node split along it, holding the larger
    /// ones, is nearer.
    std::uint8_t second_child_first{};
};

/// `ray` prepared for traversal. A zero direction component gives an infinite
/// inverse, which the slab test handles.
[[nodiscard]] inline TraversalRay traversalRay(const Ray& ray) noexcept
{
    const Vector3r d{ray.direction};
    return {.x = {.origin = ray.origin.x, .inverse_direction = 1 / d.x},
            .y = {.origin = ray.origin.y, .inverse_direction = 1 / d.y},
            .z = {.origin = ray.origin.z, .inverse_direction = 1 / d.z},
            .second_child_first = static_cast<std::uint8_t>(
                (d.x < 0 ? 1U : 0U) | (d.y < 0 ? 2U : 0U) | (d.z < 0 ? 4U : 0U))};
}

/// Factor 1 + 2 gamma(3), with gamma(n) = n u / (1 - n u) and u the unit
/// roundoff, that widens each slab's exit distance so rounding in the box test
/// cannot cull a box the ray touches. From Ize, "Robust BVH Ray Traversal",
/// Journal of Computer Graphics Techniques 2(2), 2013.
inline constexpr Real box_exit_widening{1 + 2 * (3 * std::numeric_limits<Real>::epsilon() / 2) /
                                                (1 - 3 * std::numeric_limits<Real>::epsilon() / 2)};

/// Narrows `range` to where the ray lies between the planes bounding `slab`.
/// When the ray runs inside such a plane, a distance is 0 * infinity = NaN; the
/// comparisons are written so a NaN leaves the range unchanged and the ray
/// counts as inside, matching the inclusive box faces. The exit distance moves
/// away from the entry whatever its sign, so ranges reaching behind the ray
/// origin are widened too.
inline void clipToSlab(Interval slab, AxisRay ray, Interval& range) noexcept
{
    Real near{(slab.min - ray.origin) * ray.inverse_direction};
    Real far{(slab.max - ray.origin) * ray.inverse_direction};
    if (near > far) {
        std::swap(near, far);
    }
    far *= far < 0 ? 2 - box_exit_widening : box_exit_widening;
    range.min = near > range.min ? near : range.min;
    range.max = far < range.max ? far : range.max;
}

/// True when the ray passes through `box` (faces included) with `t` in
/// `[range.min, range.max]`.
[[nodiscard]] inline bool entersBox(const Bounds3r& box, const TraversalRay& ray,
                                    Interval range) noexcept
{
    clipToSlab({.min = box.min.x, .max = box.max.x}, ray.x, range);
    clipToSlab({.min = box.min.y, .max = box.max.y}, ray.y, range);
    clipToSlab({.min = box.min.z, .max = box.max.z}, ray.z, range);
    return range.min <= range.max;
}

/// The two children of an interior node, nearer one first.
struct ChildOrder {
    std::size_t near{};
    std::size_t far{};
};

/// The children of the interior node at `index`, ordered for `ray`.
[[nodiscard]] inline ChildOrder orderedChildren(std::size_t index, const BvhNode& node,
                                                const TraversalRay& ray) noexcept
{
    assert(node.axis < 3 && "split axis must be 0, 1 or 2");
    const std::size_t first{index + 1};
    const std::size_t second{node.offset};
    if (((ray.second_child_first >> node.axis) & 1U) != 0) {
        return {.near = second, .far = first};
    }
    return {.near = first, .far = second};
}

/// Nodes still to visit during a traversal. Holds one entry per tree level at
/// most, so `bvh_max_depth` entries always suffice; the bounds-checked access
/// turns a violated depth bound into an exception rather than a stack overrun.
class NodeStack {
public:
    /// Adds a node to visit later.
    void push(std::size_t node)
    {
        assert(size < entries.size() && "tree deeper than its depth bound");
        entries.at(size++) = node;
    }

    /// True when nothing is left to visit.
    [[nodiscard]] bool empty() const noexcept { return size == 0; }

    /// Removes and returns the node added last. Precondition: not empty.
    [[nodiscard]] std::size_t pop()
    {
        assert(!empty() && "no node left to visit");
        return entries.at(--size);
    }

private:
    std::array<std::size_t, bvh_max_depth> entries{};
    std::size_t size{};
};

} // namespace detail

/// A bounding volume hierarchy over primitives known only by their boxes, built
/// with the surface area heuristic. Queries call back to test the primitives
/// whose boxes a ray reaches, so the hierarchy needs no knowledge of shapes.
class Bvh {
public:
    /// Deepest a tree can be. The build switches from surface area splits to
    /// median splits below a fixed depth, which halve the primitive count, so no
    /// tree over fewer than 2^64 primitives goes deeper than this.
    static constexpr std::size_t max_depth{detail::bvh_max_depth};

    /// A hierarchy over nothing: queries find no hits.
    Bvh() = default;

    /// Builds a hierarchy over primitives `0` to `primitive_bounds.size() - 1`.
    /// Throws `std::invalid_argument` if a box is empty or has a non-finite
    /// coordinate, and `std::bad_alloc` if memory runs out.
    explicit Bvh(std::span<const Bounds3r> primitive_bounds);

    /// The index of the primitive with the nearest hit with `t` strictly inside
    /// `t_range`, if any. Nearer subtrees are searched first and the range shrinks
    /// to each hit found, so most primitives are never tested. Throws whatever
    /// `intersect` throws.
    template <PrimitiveIntersector Intersect>
    [[nodiscard]] std::optional<std::size_t> nearestHit(const Ray& ray, Interval t_range,
                                                        Intersect intersect) const;

    /// True when any primitive is hit with `t` strictly inside `t_range`; stops
    /// at the first one found. Throws whatever `intersect` throws.
    template <PrimitiveIntersector Intersect>
    [[nodiscard]] bool anyHit(const Ray& ray, Interval t_range, Intersect intersect) const;

private:
    // Calls `visit(primitive)` for each primitive in a leaf whose box the ray
    // enters within `range`, re-reading `range` at every node so callers can
    // shrink it. Stops when `visit` returns true.
    template <typename Visit>
    void traverse(const Ray& ray, const Interval& range, Visit visit) const;

    // Calls `visit` for each primitive of a leaf; true when it asked to stop.
    template <typename Visit>
    bool visitLeaf(const detail::BvhNode& leaf, Visit& visit) const;

    std::vector<detail::BvhNode> nodes;
    // Primitive indices in leaf order: a leaf covers `count` entries from `offset`.
    std::vector<std::size_t> primitive_order;
};

template <PrimitiveIntersector Intersect>
std::optional<std::size_t> Bvh::nearestHit(const Ray& ray, Interval t_range,
                                           Intersect intersect) const
{
    std::optional<std::size_t> nearest;
    Interval remaining{t_range};
    traverse(ray, remaining, [&](std::size_t primitive) {
        if (const auto t{intersect(primitive, remaining)}) {
            assert(remaining.surrounds(*t) && "a hit must lie inside the range given");
            nearest = primitive;
            remaining.max = *t;
        }
        return false;
    });
    return nearest;
}

template <PrimitiveIntersector Intersect>
bool Bvh::anyHit(const Ray& ray, Interval t_range, Intersect intersect) const
{
    bool found{false};
    traverse(ray, t_range, [&](std::size_t primitive) {
        found = intersect(primitive, t_range).has_value();
        return found;
    });
    return found;
}

template <typename Visit>
void Bvh::traverse(const Ray& ray, const Interval& range, Visit visit) const
{
    if (nodes.empty()) {
        return;
    }
    const detail::TraversalRay traversal{detail::traversalRay(ray)};
    detail::NodeStack pending;
    std::size_t current{0};
    while (true) {
        const detail::BvhNode& node{nodes[current]};
        if (detail::entersBox(node.bounds, traversal, range)) {
            if (node.count == 0) {
                const detail::ChildOrder children{
                    detail::orderedChildren(current, node, traversal)};
                pending.push(children.far);
                current = children.near;
                continue;
            }
            if (visitLeaf(node, visit)) {
                return;
            }
        }
        if (pending.empty()) {
            return;
        }
        current = pending.pop();
    }
}

template <typename Visit>
bool Bvh::visitLeaf(const detail::BvhNode& leaf, Visit& visit) const
{
    for (std::size_t entry{leaf.offset}; entry < leaf.offset + leaf.count; ++entry) {
        if (visit(primitive_order[entry])) {
            return true;
        }
    }
    return false;
}

} // namespace albedo
