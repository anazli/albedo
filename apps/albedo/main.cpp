#include "albedo/camera/perspective_camera.hpp"
#include "albedo/color/rgb.hpp"
#include "albedo/core/bounds3.hpp"
#include "albedo/core/point3.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/transform.hpp"
#include "albedo/core/vector3.hpp"
#include "albedo/estimator/path_estimator.hpp"
#include "albedo/geometry/mesh_geometry.hpp"
#include "albedo/geometry/mesh_instance.hpp"
#include "albedo/geometry/obj.hpp"
#include "albedo/geometry/sphere.hpp"
#include "albedo/geometry/triangle.hpp"
#include "albedo/geometry/triangle_mesh.hpp"
#include "albedo/image/pixel_buffer.hpp"
#include "albedo/image/ppm.hpp"
#include "albedo/light/point_light.hpp"
#include "albedo/light/sky_light.hpp"
#include "albedo/material/conductor_material.hpp"
#include "albedo/material/dielectric_material.hpp"
#include "albedo/material/diffuse_material.hpp"
#include "albedo/material/material.hpp"
#include "albedo/render/render_image.hpp"
#include "albedo/scene/scene.hpp"
#include "albedo/texture/checker_texture.hpp"
#include "albedo/texture/constant_texture.hpp"
#include "albedo/texture/texture.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

constexpr int default_image_width{960};
constexpr int min_image_width{16};
// Deliberate cap on what the command line can request: 8192 x 4608 pixels need
// about 1.2 GB of accumulators (32 bytes per pixel), within PixelBuffer's limit.
constexpr int max_image_width{8192};
constexpr int samples_per_axis{16};
constexpr albedo::Real aspect_ratio{16.0 / 9.0};

constexpr std::string_view usage{
    "usage: albedo <output.ppm> [width] [mesh.obj]\n"
    "  width: image width in pixels, 16 to 8192 (default 960)\n"
    "  mesh.obj: a Wavefront OBJ mesh to show three times, in place of the glass and\n"
    "            gold spheres, on a checkered floor\n"};

// Parses the optional width argument. The whole string must be a base-10 integer
// in range; anything else (signs, spaces, trailing text, overflow) is rejected.
std::optional<int> parseImageWidth(std::string_view text)
{
    int width{};
    const auto [end, error]{std::from_chars(text.data(), text.data() + text.size(), width)};
    if (error != std::errc{} || end != text.data() + text.size() || width < min_image_width ||
        width > max_image_width) {
        return std::nullopt;
    }
    return width;
}

// Complex refractive indices of the noble metals, measured by Johnson and
// Christy, "Optical Constants of the Noble Metals", Phys. Rev. B 6, 4370 (1972),
// taken from the refractiveindex.info database and linearly interpolated at
// 650, 550 and 450 nm. Using one wavelength per channel is a simplification:
// each RGB channel really covers a band of the spectrum.
constexpr albedo::Rgb gold_eta{0.156, 0.424, 1.383};
constexpr albedo::Rgb gold_k{3.602, 2.472, 1.915};
constexpr albedo::Rgb copper_eta{0.238, 1.007, 1.240};
constexpr albedo::Rgb copper_k{3.626, 2.582, 2.393};
constexpr albedo::Rgb silver_eta{0.052, 0.060, 0.040};
constexpr albedo::Rgb silver_k{4.409, 3.597, 2.648};

// Walls are spheres this large: within the room their curvature is far below
// a pixel, so they read as planes until triangles arrive.
constexpr albedo::Real wall_radius{1e4};
// The room spans x in [-room_half_width, room_half_width], y in [0, room_height]
// and z from back_wall_z towards the camera, where it is open.
constexpr albedo::Real room_half_width{3.0};
constexpr albedo::Real room_height{5.0};
constexpr albedo::Real back_wall_z{-4.0};

// Which scene the app builds: the showcase, or the meshes scene, which shows an
// OBJ mesh several times and adds textures.
enum class SceneKind : std::uint8_t { Showcase, Meshes };

// Floor tiles per unit of length in the meshes scene.
constexpr albedo::Real floor_tiles_per_unit{1.25};

// A flat floor of two triangles under the whole room. Its texture coordinates
// are its x and z coordinates, so a checker texture gives square tiles.
albedo::TriangleMesh floorQuad()
{
    constexpr albedo::Real x0{-room_half_width - 0.5};
    constexpr albedo::Real x1{room_half_width + 0.5};
    constexpr albedo::Real z0{back_wall_z - 0.5};
    constexpr albedo::Real z1{12.0};
    // Counter-clockwise seen from above, so the floor faces up.
    return albedo::TriangleMesh{{{x0, 0.0, z1}, {x1, 0.0, z1}, {x1, 0.0, z0}, {x0, 0.0, z0}},
                                {{0, 1, 2}, {0, 2, 3}},
                                {{x0, z1}, {x1, z1}, {x1, z0}, {x0, z0}},
                                {{0, 1, 2}, {0, 2, 3}}};
}

// A room open towards the camera: white floor (grey and white tiles in the
// meshes scene), ceiling and back wall, a red wall on the left and a green one
// on the right (after the Cornell box).
void addRoom(albedo::Scene& scene, SceneKind kind)
{
    using albedo::DiffuseMaterial;
    using albedo::Point3r;
    using albedo::Sphere;
    const DiffuseMaterial white{.albedo = albedo::ConstantTexture{.color = {0.73, 0.73, 0.73}}};
    if (kind == SceneKind::Meshes) {
        scene.add(floorQuad(), DiffuseMaterial{.albedo = albedo::CheckerTexture{
                                                   .even = {0.73, 0.73, 0.73},
                                                   .odd = {0.3, 0.3, 0.3},
                                                   .frequency = floor_tiles_per_unit}});
    } else {
        scene.add(Sphere{Point3r{0.0, -wall_radius, 0.0}, wall_radius}, white);
    }
    scene.add(Sphere{Point3r{0.0, room_height + wall_radius, 0.0}, wall_radius}, white);
    scene.add(Sphere{Point3r{0.0, 0.0, back_wall_z - wall_radius}, wall_radius}, white);
    scene.add(Sphere{Point3r{-room_half_width - wall_radius, 0.0, 0.0}, wall_radius},
              DiffuseMaterial{.albedo = albedo::ConstantTexture{.color = {0.63, 0.07, 0.05}}});
    scene.add(Sphere{Point3r{room_half_width + wall_radius, 0.0, 0.0}, wall_radius},
              DiffuseMaterial{.albedo = albedo::ConstantTexture{.color = {0.12, 0.45, 0.15}}});
}

// The two largest spheres, in the middle of the room: smooth glass and rough gold.
void addCentralSpheres(albedo::Scene& scene)
{
    using albedo::Point3r;
    using albedo::Sphere;
    scene.add(Sphere{Point3r{-0.9, 1.0, -1.0}, 1.0},
              albedo::DielectricMaterial{.ior = 1.5, .roughness = 0.0});
    scene.add(Sphere{Point3r{1.6, 0.8, -2.2}, 0.8},
              albedo::ConductorMaterial{.eta = gold_eta, .k = gold_k, .roughness = 0.25});
}

// The blue sphere's surface: plain in the showcase, and in the meshes scene
// checkered in latitude and longitude, which shows the sphere's texture coordinates.
albedo::Texture blueSphereTexture(SceneKind kind)
{
    constexpr albedo::Rgb blue{0.15, 0.3, 0.7};
    if (kind == SceneKind::Meshes) {
        return albedo::CheckerTexture{.even = blue, .odd = {0.8, 0.8, 0.8}, .frequency = 8.0};
    }
    return albedo::ConstantTexture{.color = blue};
}

// Smaller spheres around the edges: a silver mirror, frosted glass, rough
// copper and matte blue.
void addOuterSpheres(albedo::Scene& scene, SceneKind kind)
{
    using albedo::ConductorMaterial;
    using albedo::DielectricMaterial;
    using albedo::Point3r;
    using albedo::Sphere;
    scene.add(Sphere{Point3r{-1.9, 0.6, 1.3}, 0.6},
              ConductorMaterial{.eta = silver_eta, .k = silver_k, .roughness = 0.0});
    scene.add(Sphere{Point3r{1.8, 0.45, 0.8}, 0.45},
              DielectricMaterial{.ior = 1.5, .roughness = 0.35});
    scene.add(Sphere{Point3r{0.9, 0.35, 2.3}, 0.35},
              ConductorMaterial{.eta = copper_eta, .k = copper_k, .roughness = 0.3});
    scene.add(Sphere{Point3r{-2.2, 0.5, -2.8}, 0.5},
              albedo::DiffuseMaterial{.albedo = blueSphereTexture(kind)});
}

// A warm light set into the ceiling with only its lowest part showing below it
// (soft shadows and a caustic under the glass), a small cool point light (hard
// shadows) and a faint sky through the open front.
void addLights(albedo::Scene& scene)
{
    scene.addAreaLight(
        albedo::Sphere{albedo::Point3r{0.0, room_height + 0.6, -1.2}, 1.1}, {9.0, 7.6, 5.8},
        albedo::DiffuseMaterial{.albedo = albedo::ConstantTexture{.color = {0.0, 0.0, 0.0}}});
    scene.addLight(albedo::PointLight{.position = {0.8, 4.2, 3.2}, .intensity = {1.2, 2.0, 3.6}});
    scene.setSky(albedo::SkyLight::uniform({0.05, 0.06, 0.08}));
}

// Where one copy of the mesh goes: scaled so the box around it has its longest
// side `size` long, turned by `degrees` about `axis`, and standing on the floor
// with the box centred over (`x`, `z`).
struct MeshPlacement {
    albedo::Real x{};
    albedo::Real z{};
    albedo::Real size{};
    albedo::Vector3r axis{0.0, 1.0, 0.0};
    albedo::Real degrees{};
    albedo::Material material;
};

// The three copies of the mesh in the meshes scene: rough gold at the back,
// clear glass at the front left and rough copper on the right, each turned
// differently, leaving the checkered blue sphere in view.
std::vector<MeshPlacement> meshPlacements()
{
    return {
        {.x = 0.1,
         .z = -2.3,
         .size = 2.6,
         .axis = {0.0, 1.0, 0.0},
         .degrees = 20.0,
         .material = albedo::ConductorMaterial{.eta = gold_eta, .k = gold_k, .roughness = 0.15}},
        {.x = -0.75,
         .z = 0.35,
         .size = 1.3,
         .axis = {1.0, 0.0, 0.0},
         .degrees = 70.0,
         .material = albedo::DielectricMaterial{.ior = 1.5, .roughness = 0.0}},
        {.x = 1.45,
         .z = -0.7,
         .size = 1.3,
         .axis = {0.3, 0.2, 1.0},
         .degrees = -40.0,
         .material =
             albedo::ConductorMaterial{.eta = copper_eta, .k = copper_k, .roughness = 0.3}}};
}

// A mesh read from a file: its shared geometry, and the corners of every
// triangle it keeps, for finding where a placement puts its lowest point.
struct LoadedMesh {
    std::shared_ptr<const albedo::MeshGeometry> geometry;
    std::vector<albedo::Point3r> corners;
};

// The mesh in the OBJ file at `path`. `fail` makes the error to throw, naming
// the file.
template <typename Fail>
LoadedMesh loadMesh(const std::filesystem::path& path, const Fail& fail)
{
    std::optional<albedo::TriangleMesh> mesh;
    try {
        mesh = albedo::readObj(path);
    } catch (const albedo::ObjParseError& error) {
        throw fail(error.what());
    }
    // Checked here, so MeshGeometry's own invalid_argument is not mistaken for it.
    const std::vector<albedo::Triangle> triangles{mesh->triangles()};
    if (triangles.empty()) {
        throw fail("the mesh has no triangles");
    }
    std::vector<albedo::Point3r> corners;
    corners.reserve(3 * triangles.size());
    for (const albedo::Triangle& triangle : triangles) {
        corners.insert(corners.end(), triangle.vertices().begin(), triangle.vertices().end());
    }
    return {.geometry = std::make_shared<const albedo::MeshGeometry>(*mesh),
            .corners = std::move(corners)};
}

// The lowest height `transform` puts any of `corners` at: +infinity if
// `corners` is empty or every height is NaN, and -infinity if one overflows.
albedo::Real lowestHeight(const albedo::Transform& transform,
                          std::span<const albedo::Point3r> corners) noexcept
{
    albedo::Real lowest{std::numeric_limits<albedo::Real>::infinity()};
    for (const albedo::Point3r& corner : corners) {
        lowest = std::min(lowest, transform.apply(corner).y);
    }
    return lowest;
}

// `mesh` scaled uniformly, turned and stood on the floor as `placement` says.
// `fail` makes the error to throw, naming the file, when it cannot be placed.
template <typename Fail>
albedo::MeshInstance fittedInstance(const LoadedMesh& mesh, const MeshPlacement& placement,
                                    const Fail& fail)
{
    const albedo::Bounds3r box{mesh.geometry->bounds()};
    const albedo::Vector3r extent{box.extent()};
    const albedo::Real scale{placement.size / std::max({extent.x, extent.y, extent.z})};
    if (!std::isfinite(scale) || !(scale > 0)) {
        throw fail("the mesh is too large to scale into the room");
    }
    // Halved before adding, so huge coordinates cannot overflow.
    const albedo::Vector3r box_center{box.min.x / 2 + box.max.x / 2, box.min.y / 2 + box.max.y / 2,
                                      box.min.z / 2 + box.max.z / 2};
    // One affine map holds the whole placement, so a mesh centred far enough from
    // the origin overflows it even when the placed mesh itself would fit.
    const auto unplaceable{[&] {
        return fail("the mesh is too large or too far from the origin to place in the room");
    }};
    try {
        const albedo::Transform turned{
            albedo::Transform::rotation(placement.axis, placement.degrees) *
            albedo::Transform::scaling({scale, scale, scale}) *
            albedo::Transform::translation(-box_center)};
        // Lifted so the turned mesh's lowest corner touches the floor; its bounding
        // box would reach lower than the mesh once turned.
        const albedo::Real lift{-lowestHeight(turned, mesh.corners)};
        if (!std::isfinite(lift)) {
            throw unplaceable();
        }
        const albedo::Transform to_room{
            albedo::Transform::translation({placement.x, lift, placement.z}) * turned};
        return albedo::MeshInstance{mesh.geometry, to_room};
    } catch (const std::invalid_argument&) {
        throw unplaceable();
    }
}

// Adds the copies of the mesh at `path`, which share one geometry. Throws
// `std::runtime_error` naming the file if it cannot be read or has nothing to show.
void addMeshes(albedo::Scene& scene, const std::filesystem::path& path)
{
    const auto fail{[&](std::string_view problem) {
        return std::runtime_error{std::format("{}: {}", path.string(), problem)};
    }};
    const LoadedMesh mesh{loadMesh(path, fail)};
    for (const MeshPlacement& placement : meshPlacements()) {
        scene.add(fittedInstance(mesh, placement, fail), placement.material);
    }
}

// The showcase room; with a mesh, the meshes scene, whose copies of the mesh
// replace the two central spheres.
albedo::Scene buildScene(const std::optional<std::filesystem::path>& mesh_path)
{
    const SceneKind kind{mesh_path ? SceneKind::Meshes : SceneKind::Showcase};
    albedo::Scene scene;
    addRoom(scene, kind);
    if (mesh_path) {
        addMeshes(scene, *mesh_path);
    } else {
        addCentralSpheres(scene);
    }
    addOuterSpheres(scene, kind);
    addLights(scene);
    return scene;
}

albedo::PerspectiveCamera buildCamera(int width, int height)
{
    return albedo::PerspectiveCamera{
        albedo::CameraSettings{.look_from = {0.0, 2.4, 7.8},
                               .look_at = {0.0, 1.9, -0.5},
                               .up = {0.0, 1.0, 0.0},
                               .vertical_fov_degrees = 38.0,
                               .aspect_ratio = static_cast<albedo::Real>(width) / height}};
}

// Renders the scene into a `width` x (`width` * 9 / 16) image and writes it to `output`.
void renderTo(const std::filesystem::path& output, int width,
              const std::optional<std::filesystem::path>& mesh_path)
{
    const int height{std::max(1, static_cast<int>(std::lround(width / aspect_ratio)))};
    const albedo::Scene scene{buildScene(mesh_path)};
    const albedo::PerspectiveCamera camera{buildCamera(width, height)};
    albedo::PixelBuffer image{width, height};
    albedo::renderImage({.camera = camera,
                         .scene = scene,
                         .samples_per_axis = samples_per_axis,
                         .estimator = albedo::PathEstimator{},
                         .thread_count = albedo::availableThreadCount()},
                        image);
    albedo::writePpm(output, image);
}

} // namespace

int main(int argc, char* argv[])
{
    const std::span args{argv, static_cast<std::size_t>(argc)};
    if (args.size() < 2 || args.size() > 4) {
        std::cerr << usage;
        return EXIT_FAILURE;
    }
    const std::optional<int> width{args.size() >= 3 ? parseImageWidth(args[2])
                                                    : default_image_width};
    if (!width) {
        std::cerr << std::format("albedo: invalid width '{}'\n{}", args[2], usage);
        return EXIT_FAILURE;
    }

    try {
        // On Windows `argv` is in the ANSI code page, so a path with characters
        // outside it arrives mangled. Supporting that needs `wmain`; not worth it yet.
        const std::filesystem::path output{args[1]};
        const std::optional<std::filesystem::path> mesh_path{
            args.size() == 4 ? std::optional<std::filesystem::path>{args[3]} : std::nullopt};
        renderTo(output, *width, mesh_path);
        std::cout << std::format("albedo: wrote {}\n", output.string());
    } catch (const std::exception& error) {
        std::cerr << std::format("albedo: {}\n", error.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
