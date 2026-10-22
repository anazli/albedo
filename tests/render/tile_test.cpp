#include "albedo/render/tile.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace albedo {
namespace {

// How many tiles cover each pixel of a `width` x `height` image, row-major.
std::vector<int> coverageCounts(const std::vector<Tile>& tiles, int width, int height)
{
    std::vector<int> counts(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (const Tile& tile : tiles) {
        for (int y{tile.min.y}; y < tile.max.y; ++y) {
            for (int x{tile.min.x}; x < tile.max.x; ++x) {
                ++counts.at(static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                            static_cast<std::size_t>(x));
            }
        }
    }
    return counts;
}

// Runs the loop on one thread with work that always throws. Returns how many
// tiles were started, or nothing if the exception was not rethrown.
std::optional<int> tilesStartedBeforeRethrow(std::span<const Tile> tiles)
{
    int calls{0};
    try {
        forEachTileInParallel(tiles, 1, [&](const Tile& /*tile*/) {
            ++calls;
            throw std::runtime_error{"tile failed"};
        });
    } catch (const std::runtime_error&) {
        return calls;
    }
    return std::nullopt;
}

TEST(TileTest, tilesCoverEveryPixelExactlyOnce)
{
    // Neither side is a multiple of the tile size, so edge tiles are clipped.
    const std::vector<Tile> tiles{splitIntoTiles(37, 21, 16)};

    for (const int count : coverageCounts(tiles, 37, 21)) {
        ASSERT_EQ(count, 1);
    }
}

TEST(TileTest, edgeTilesAreClippedToTheImage)
{
    const std::vector<Tile> tiles{splitIntoTiles(37, 21, 16)};

    ASSERT_EQ(tiles.size(), 6U);
    EXPECT_EQ(tiles.back(), (Tile{.min = {.x = 32, .y = 16}, .max = {.x = 37, .y = 21}}));
}

TEST(TileTest, tilesAreInRowMajorOrder)
{
    const std::vector<Tile> tiles{splitIntoTiles(37, 21, 16)};

    EXPECT_EQ(tiles[0], (Tile{.min = {.x = 0, .y = 0}, .max = {.x = 16, .y = 16}}));
    EXPECT_EQ(tiles[1], (Tile{.min = {.x = 16, .y = 0}, .max = {.x = 32, .y = 16}}));
    EXPECT_EQ(tiles[3], (Tile{.min = {.x = 0, .y = 16}, .max = {.x = 16, .y = 21}}));
}

TEST(TileTest, splitDoesNotOverflowNearTheLargestInt)
{
    // Stepping by the full tile size past the last tile would overflow `int`,
    // which UBSan reports.
    constexpr int largest{std::numeric_limits<int>::max()};

    const std::vector<Tile> tiles{splitIntoTiles(largest, 1, largest - 1)};

    ASSERT_EQ(tiles.size(), 2U);
    EXPECT_EQ(tiles[1], (Tile{.min = {.x = largest - 1, .y = 0}, .max = {.x = largest, .y = 1}}));
}

TEST(TileTest, imageSmallerThanATileIsOneTile)
{
    const std::vector<Tile> tiles{splitIntoTiles(5, 3, 16)};

    ASSERT_EQ(tiles.size(), 1U);
    EXPECT_EQ(tiles[0], (Tile{.min = {.x = 0, .y = 0}, .max = {.x = 5, .y = 3}}));
}

TEST(TileTest, splitRejectsNonPositiveSizes)
{
    EXPECT_THROW(static_cast<void>(splitIntoTiles(8, 8, 0)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(splitIntoTiles(0, 8, 4)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(splitIntoTiles(8, -1, 4)), std::invalid_argument);
}

TEST(TileTest, parallelLoopVisitsEveryTileOnceForAnyThreadCount)
{
    const std::vector<Tile> tiles{splitIntoTiles(100, 70, 8)};

    for (const int thread_count : {1, 2, 3, 8, 1000}) {
        SCOPED_TRACE(::testing::Message() << thread_count << " threads");
        std::vector<std::atomic<int>> visits(tiles.size());
        forEachTileInParallel(tiles, thread_count, [&](const Tile& tile) {
            const auto index{static_cast<std::size_t>(&tile - tiles.data())};
            ++visits.at(index);
        });
        for (const std::atomic<int>& count : visits) {
            ASSERT_EQ(count.load(), 1);
        }
    }
}

TEST(TileTest, parallelLoopOverNoTilesDoesNothing)
{
    int calls{0};

    forEachTileInParallel({}, 4, [&](const Tile& /*tile*/) { ++calls; });

    EXPECT_EQ(calls, 0);
}

TEST(TileTest, parallelLoopRethrowsAWorkerException)
{
    const std::vector<Tile> tiles{splitIntoTiles(64, 64, 8)};

    const auto fail_on_sixth_tile{[&](const Tile& tile) {
        if (&tile == &tiles[5]) {
            throw std::runtime_error{"tile failed"};
        }
    }};

    EXPECT_THROW(forEachTileInParallel(tiles, 4, fail_on_sixth_tile), std::runtime_error);
}

TEST(TileTest, parallelLoopStopsHandingOutTilesAfterAnException)
{
    // With one thread the order is fixed: the first tile fails, no other starts.
    const std::vector<Tile> tiles{splitIntoTiles(64, 64, 8)};

    EXPECT_EQ(tilesStartedBeforeRethrow(tiles), std::optional<int>{1});
}

TEST(TileTest, parallelLoopRejectsNonPositiveThreadCount)
{
    const std::vector<Tile> tiles{splitIntoTiles(8, 8, 4)};

    EXPECT_THROW(forEachTileInParallel(tiles, 0, [](const Tile& /*tile*/) {}),
                 std::invalid_argument);
}

} // namespace
} // namespace albedo
