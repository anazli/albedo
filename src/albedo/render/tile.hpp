#pragma once

#include "albedo/core/point2.hpp"

#include <functional>
#include <span>
#include <vector>

namespace albedo {

/// A rectangle of pixels from `min` (inclusive) to `max` (exclusive).
struct Tile {
    /// Top-left pixel of the tile.
    Point2i min{};
    /// One past the bottom-right pixel of the tile.
    Point2i max{};

    [[nodiscard]] constexpr bool operator==(const Tile&) const noexcept = default;
};

/// Splits a `width` x `height` image into `tile_size` x `tile_size` tiles in
/// row-major order, clipping those on the right and bottom edges. Every pixel
/// lies in exactly one tile. Throws `std::invalid_argument` unless all three
/// arguments are positive, `std::length_error` if the tile count exceeds what a
/// vector can hold, and `std::bad_alloc` if the list cannot be allocated.
[[nodiscard]] std::vector<Tile> splitIntoTiles(int width, int height, int tile_size);

/// Calls `work` once for every tile, on up to `thread_count` threads including
/// the calling one, passing a reference to the element of `tiles` itself. Threads take the next
/// unclaimed tile until none are left, so the assignment of tiles to threads varies between runs;
/// `work` must give the same result whichever thread runs it, and calls on different tiles must be
/// safe to run concurrently.
///
/// If `work` throws, no further tiles are started, the threads are joined and
/// the first exception is rethrown. Throws `std::invalid_argument` if
/// `thread_count` is not positive, and `std::system_error` if a thread cannot
/// be started (after the threads already started have finished).
void forEachTileInParallel(std::span<const Tile> tiles, int thread_count,
                           const std::function<void(const Tile&)>& work);

} // namespace albedo
