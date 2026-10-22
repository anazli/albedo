#include "albedo/render/tile.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <exception>
#include <format>
#include <stdexcept>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

namespace albedo {

namespace {

// Number of tiles needed to cover `length` pixels; both arguments are positive.
int tilesAlong(int length, int tile_size) noexcept
{
    return (length - 1) / tile_size + 1;
}

// Hands out tiles to the threads and records the first failure.
class TileQueue {
public:
    // Not `noexcept`: the default `std::stop_source` allocates its shared state.
    explicit TileQueue(std::span<const Tile> tiles) : tiles{tiles} {}

    // Runs `work` on tiles until none are left or one has failed anywhere. Cannot
    // throw, so an exception never escapes a thread and terminates the program.
    void drain(const std::function<void(const Tile&)>& work) noexcept
    {
        while (!stop.stop_requested()) {
            const std::size_t index{next_tile.fetch_add(1)};
            if (index >= tiles.size()) {
                return;
            }
            try {
                work(tiles[index]);
            } catch (...) {
                recordFailure(std::current_exception());
            }
        }
    }

    // Lets the threads finish their current tile and start no other.
    void cancel() noexcept { stop.request_stop(); }

    // Rethrows the first failure, if any. Call only after every thread has
    // finished; joining them makes their write to `failure` visible here.
    void rethrowFailure() const
    {
        if (failure) {
            std::rethrow_exception(failure);
        }
    }

private:
    // The first thread to claim the slot stores its exception; no other thread
    // touches `failure`, so it needs no lock (and a lock could throw).
    void recordFailure(std::exception_ptr error) noexcept
    {
        bool already_claimed{false};
        if (failure_claimed.compare_exchange_strong(already_claimed, true)) {
            failure = std::move(error);
        }
        stop.request_stop();
    }

    std::span<const Tile> tiles;
    std::atomic<std::size_t> next_tile{0};
    std::stop_source stop;
    std::atomic<bool> failure_claimed{false};
    std::exception_ptr failure;
};

} // namespace

std::vector<Tile> splitIntoTiles(int width, int height, int tile_size)
{
    if (width <= 0 || height <= 0 || tile_size <= 0) {
        throw std::invalid_argument{
            std::format("image and tile sizes must be positive, got {}x{} with tile size {}", width,
                        height, tile_size)};
    }
    std::vector<Tile> tiles;
    tiles.reserve(static_cast<std::size_t>(tilesAlong(width, tile_size)) *
                  static_cast<std::size_t>(tilesAlong(height, tile_size)));
    // Each step is clipped to what remains, so `x` and `y` never pass the image
    // edge and cannot overflow even for sizes near INT_MAX.
    for (int y{0}; y < height;) {
        const int tile_height{std::min(tile_size, height - y)};
        for (int x{0}; x < width;) {
            const int tile_width{std::min(tile_size, width - x)};
            tiles.push_back(
                {.min = {.x = x, .y = y}, .max = {.x = x + tile_width, .y = y + tile_height}});
            x += tile_width;
        }
        y += tile_height;
    }
    return tiles;
}

void forEachTileInParallel(std::span<const Tile> tiles, int thread_count,
                           const std::function<void(const Tile&)>& work)
{
    if (thread_count <= 0) {
        throw std::invalid_argument{
            std::format("thread count must be positive, got {}", thread_count)};
    }
    TileQueue queue{tiles};
    // More threads than tiles would only start threads with nothing to do.
    const std::size_t extra_threads{
        std::min(static_cast<std::size_t>(thread_count), std::max<std::size_t>(tiles.size(), 1)) -
        1};
    {
        std::vector<std::jthread> threads;
        threads.reserve(extra_threads);
        try {
            for (std::size_t i{0}; i < extra_threads; ++i) {
                threads.emplace_back([&queue, &work] { queue.drain(work); });
            }
        } catch (...) {
            // The started threads are joined while the exception propagates.
            queue.cancel();
            throw;
        }
        queue.drain(work);
    } // The jthreads join here, before the failure is read.
    queue.rethrowFailure();
}

} // namespace albedo
