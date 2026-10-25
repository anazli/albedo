#include "albedo/accel/bvh.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace albedo {

namespace {

// The build follows the binned surface area heuristic of Wald, "On fast
// Construction of SAH-based Bounding Volume Hierarchies", IEEE Symposium on
// Interactive Ray Tracing, 2007: centroids are sorted into equal-width bins
// along each axis and only the bin boundaries are considered as split planes.

constexpr std::size_t bin_count{12};
// Cost of visiting a node relative to testing one primitive. A typical ratio,
// not a measured one; it only shifts how eagerly the build makes leaves.
constexpr Real traversal_cost{0.125};
// Leaves stay small so a ray never tests many primitives at once.
constexpr std::size_t max_leaf_size{4};
// Below this depth only median splits are made, which bounds the tree depth by
// this plus log2 of the primitive count (see Bvh::max_depth).
constexpr std::size_t surface_area_depth_limit{32};

static_assert(surface_area_depth_limit + 64 < Bvh::max_depth);

struct BuildPrimitive {
    Bounds3r bounds{};
    Point3r centroid{};
    std::size_t index{};
};

Point3r centroidOf(const Bounds3r& box) noexcept
{
    // Halved before adding so huge coordinates cannot overflow.
    return {box.min.x / 2 + box.max.x / 2, box.min.y / 2 + box.max.y / 2,
            box.min.z / 2 + box.max.z / 2};
}

// The coordinate of a point or vector along axis 0, 1 or 2.
template <typename Tuple>
Real axisValue(Tuple p, std::size_t axis) noexcept
{
    switch (axis) {
    case 0:
        return p.x;
    case 1:
        return p.y;
    default:
        return p.z;
    }
}

struct Extents {
    Bounds3r bounds{};
    Bounds3r centroids{};
};

Extents extentsOf(std::span<const BuildPrimitive> primitives) noexcept
{
    Extents extents;
    for (const BuildPrimitive& primitive : primitives) {
        extents.bounds = merged(extents.bounds, primitive.bounds);
        extents.centroids = merged(extents.centroids, primitive.centroid);
    }
    return extents;
}

std::size_t widestAxis(const Bounds3r& box) noexcept
{
    const Vector3r extent{box.extent()};
    if (extent.x >= extent.y && extent.x >= extent.z) {
        return 0;
    }
    return extent.y >= extent.z ? 1 : 2;
}

// Half the distance from `low` to `high`. Halving first keeps the result finite
// for any finite inputs, where `high - low` itself can overflow.
Real halfDistance(Real low, Real high) noexcept
{
    return high / 2 - low / 2;
}

// Half the spread of `centroids` along `axis`.
Real halfSpread(const Bounds3r& centroids, std::size_t axis) noexcept
{
    return halfDistance(axisValue(centroids.min, axis), axisValue(centroids.max, axis));
}

// Which bin of `centroids` along `axis` a centroid falls in. The smallest
// centroid lands in the first bin and the largest in the last, since its offset
// and the spread are the same expression.
// Precondition: `halfSpread(centroids, axis) > 0` and the centroid lies in `centroids`.
std::size_t binOf(Point3r centroid, const Bounds3r& centroids, std::size_t axis) noexcept
{
    const Real low{axisValue(centroids.min, axis)};
    const Real fraction{halfDistance(low, axisValue(centroid, axis)) / halfSpread(centroids, axis)};
    assert(fraction >= 0 && fraction <= 1 && "centroid outside the centroid bounds");
    // Limited to [0, 1] before the cast, which would be undefined for a NaN or huge
    // value; the comparison maps NaN to 0, which std::clamp would pass through.
    const Real limited{fraction > 0 ? std::min(fraction, Real{1}) : Real{0}};
    const Real scaled{limited * static_cast<Real>(bin_count)};
    return std::min(static_cast<std::size_t>(scaled), bin_count - 1);
}

struct Bin {
    Bounds3r bounds{};
    std::size_t count{};
};

// A split between bins `last_left_bin` and `last_left_bin + 1` along `axis`,
// and its cost scaled by the parent's surface area.
struct Split {
    std::size_t axis{};
    std::size_t last_left_bin{};
    Real cost{std::numeric_limits<Real>::infinity()};
};

// The cheapest split along `axis`. Every boundary leaves primitives on both
// sides: the smallest centroid lands in the first bin, the largest in the last.
Split bestSplitAlong(std::span<const BuildPrimitive> primitives, const Extents& extents,
                     std::size_t axis)
{
    std::array<Bin, bin_count> bins{};
    for (const BuildPrimitive& primitive : primitives) {
        Bin& bin{bins.at(binOf(primitive.centroid, extents.centroids, axis))};
        bin.bounds = merged(bin.bounds, primitive.bounds);
        ++bin.count;
    }
    // Surface area and count of everything right of each boundary, swept from the right.
    std::array<Bin, bin_count> right_of{};
    for (std::size_t bin{bin_count - 1}; bin > 0; --bin) {
        const Bin& next{bin + 1 < bin_count ? right_of.at(bin) : Bin{}};
        right_of.at(bin - 1) = {.bounds = merged(next.bounds, bins.at(bin).bounds),
                                .count = next.count + bins.at(bin).count};
    }
    Split best{.axis = axis};
    Bin left{};
    for (std::size_t bin{0}; bin + 1 < bin_count; ++bin) {
        left = {.bounds = merged(left.bounds, bins.at(bin).bounds),
                .count = left.count + bins.at(bin).count};
        const Bin& right{right_of.at(bin)};
        const Real cost{left.bounds.surfaceArea() * static_cast<Real>(left.count) +
                        right.bounds.surfaceArea() * static_cast<Real>(right.count)};
        if (cost < best.cost) {
            best = {.axis = axis, .last_left_bin = bin, .cost = cost};
        }
    }
    return best;
}

// The cheapest split over all axes along which the centroids spread, if any.
std::optional<Split> bestSplit(std::span<const BuildPrimitive> primitives, const Extents& extents)
{
    std::optional<Split> best;
    for (std::size_t axis{0}; axis < 3; ++axis) {
        if (!(halfSpread(extents.centroids, axis) > 0)) {
            continue;
        }
        const Split split{bestSplitAlong(primitives, extents, axis)};
        if (!best || split.cost < best->cost) {
            best = split;
        }
    }
    return best;
}

// How a node divides its primitives: the first `left_count` go to the first
// child. Zero means the node becomes a leaf.
struct Partition {
    std::size_t left_count{};
    std::size_t axis{};
};

// Reorders `primitives` for the node's split.
Partition partitionForSplit(std::span<BuildPrimitive> primitives, const Extents& extents,
                            std::size_t depth)
{
    const std::size_t count{primitives.size()};
    if (depth < surface_area_depth_limit) {
        if (const auto split{bestSplit(primitives, extents)}) {
            const Real area{extents.bounds.surfaceArea()};
            // Compared after multiplying through by the area, which may be zero.
            const bool leaf_is_cheaper{static_cast<Real>(count) * area <=
                                       traversal_cost * area + split->cost};
            if (count <= max_leaf_size && leaf_is_cheaper) {
                return {};
            }
            const auto middle{std::partition(primitives.begin(), primitives.end(),
                                             [&](const BuildPrimitive& primitive) {
                                                 return binOf(primitive.centroid, extents.centroids,
                                                              split->axis) <= split->last_left_bin;
                                             })};
            return {.left_count = static_cast<std::size_t>(middle - primitives.begin()),
                    .axis = split->axis};
        }
    }
    if (count <= max_leaf_size) {
        return {};
    }
    // Coincident centroids, or deep in the tree: halve the count by position.
    const std::size_t axis{widestAxis(extents.centroids)};
    const std::size_t half{count / 2};
    const auto middle{primitives.begin() + static_cast<std::ptrdiff_t>(half)};
    std::nth_element(primitives.begin(), middle, primitives.end(),
                     [&](const BuildPrimitive& a, const BuildPrimitive& b) {
                         return axisValue(a.centroid, axis) < axisValue(b.centroid, axis);
                     });
    return {.left_count = half, .axis = axis};
}

// Appends the subtree over `primitives`, which start at `first` in the final
// primitive order, and returns the index of its root.
std::size_t buildSubtree(std::vector<detail::BvhNode>& nodes, std::span<BuildPrimitive> primitives,
                         std::size_t first, std::size_t depth)
{
    const std::size_t index{nodes.size()};
    const Extents extents{extentsOf(primitives)};
    nodes.push_back({.bounds = extents.bounds});
    const Partition partition{
        primitives.size() == 1 ? Partition{} : partitionForSplit(primitives, extents, depth)};
    const std::size_t left_count{partition.left_count};
    if (left_count == 0) {
        nodes[index].offset = first;
        nodes[index].count = static_cast<std::uint32_t>(primitives.size());
        return index;
    }
    buildSubtree(nodes, primitives.first(left_count), first, depth + 1);
    const std::size_t second{
        buildSubtree(nodes, primitives.subspan(left_count), first + left_count, depth + 1)};
    nodes[index].offset = second;
    nodes[index].axis = static_cast<std::uint8_t>(partition.axis);
    return index;
}

} // namespace

Bvh::Bvh(std::span<const Bounds3r> primitive_bounds)
{
    if (primitive_bounds.empty()) {
        return;
    }
    std::vector<BuildPrimitive> primitives;
    primitives.reserve(primitive_bounds.size());
    for (std::size_t index{0}; index < primitive_bounds.size(); ++index) {
        const Bounds3r& box{primitive_bounds[index]};
        if (!isFinite(box.min) || !isFinite(box.max) || box.isEmpty()) {
            throw std::invalid_argument{
                std::format("BVH primitive {} has an empty or non-finite box", index)};
        }
        primitives.push_back({.bounds = box, .centroid = centroidOf(box), .index = index});
    }
    // A binary tree with a leaf per primitive at most.
    nodes.reserve(2 * primitives.size() - 1);
    buildSubtree(nodes, primitives, 0, 0);
    primitive_order.reserve(primitives.size());
    for (const BuildPrimitive& primitive : primitives) {
        primitive_order.push_back(primitive.index);
    }
}

} // namespace albedo
