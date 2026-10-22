# Albedo

A physically based path tracer in C++20, built one step at a time.

![Spheres of glass, gold, copper, silver and blue paint in a red-and-green room lit by a ceiling light](scenes/showcase.png)

The image above is what the `albedo` app renders: 1920 × 1080 pixels at 256 samples per pixel. It
uses most of what the renderer can do so far:

- **Path tracing** with next-event estimation and multiple importance sampling (power heuristic),
  plus Russian roulette.
- **Lights:** a spherical area light in the ceiling (soft shadows), a point light (hard shadows)
  and a faint sky.
- **Materials:**
  - diffuse walls, whose red and green bleed onto the floor and spheres;
  - smooth glass, which focuses light into a caustic on the floor;
  - frosted glass, using rough GGX transmission;
  - a silver mirror;
  - rough gold and copper, with GGX microfacets and complex refractive indices measured by
    Johnson and Christy (1972).
- **Rendering:** stratified, jittered pixel sampling and multithreaded tile rendering, with
  images identical for any thread count.

Spheres are the only shape so far; the walls are very large spheres. Triangle meshes are next on
the [roadmap](doc/roadmap.md), which lists every step and the literature each part follows.

Earlier milestones are kept in [`scenes/`](scenes).

## Building

Requirements:

- CMake 3.24 or newer.
- A C++20 compiler whose standard library provides `std::format`, such as GCC 13 or newer.

The build is tested on Linux with GCC 13. The CMake setup also has MSVC flags (`/W4 /WX`), but
no Windows build has been run.

For rendering, use an optimized build:

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DALBEDO_BUILD_TESTS=OFF
cmake --build build-release --config Release -j
```

`--config` matters only for multi-configuration generators such as Visual Studio, which ignore
`CMAKE_BUILD_TYPE` and place the executable in a per-configuration directory
(`build-release/apps/Release/`).

If your default compiler is older, choose one explicitly, for example
`-DCMAKE_CXX_COMPILER=g++-13`.

## Rendering

```bash
./build-release/apps/albedo out.ppm 1920
```

The output is a binary PPM image; the width (16 to 8192 pixels, default 960) sets the size, and
the height follows from the 16:9 aspect ratio. Many image viewers open PPM directly; to get a PNG,
use any converter, for example ImageMagick's `magick out.ppm out.png`.

The app uses every hardware thread. On an 8-thread machine the 1920-pixel showcase takes about
four minutes.

## Tests

Debug builds compile with AddressSanitizer and UndefinedBehaviorSanitizer by default (only
AddressSanitizer on MSVC; turn them off with `-DALBEDO_ENABLE_SANITIZERS=OFF`). All warnings are
errors. The tests use GoogleTest, which is downloaded when the build is first configured, so that
step needs network access.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug -j
ctest --test-dir build -C Debug --output-on-failure
```

## License

GNU General Public License v3.0; see [LICENSE](LICENSE).
