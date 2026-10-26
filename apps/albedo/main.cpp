#include "albedo/camera/perspective_camera.hpp"
#include "albedo/color/rgb.hpp"
#include "albedo/core/bounds3.hpp"
#include "albedo/core/point3.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"
#include "albedo/estimator/path_estimator.hpp"
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
#include "albedo/render/render_image.hpp"
#include "albedo/scene/scene.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <limits>
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
    "  mesh.obj: a Wavefront OBJ mesh to show in place of the glass and gold spheres\n"};

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

// A room open towards the camera: white floor, ceiling and back wall, a red
// wall on the left and a green one on the right (after the Cornell box).
void addRoom(albedo::Scene& scene)
{
    using albedo::DiffuseMaterial;
    using albedo::Point3r;
    using albedo::Sphere;
    const DiffuseMaterial white{.albedo = {0.73, 0.73, 0.73}};
    scene.add(Sphere{Point3r{0.0, -wall_radius, 0.0}, wall_radius}, white);
    scene.add(Sphere{Point3r{0.0, room_height + wall_radius, 0.0}, wall_radius}, white);
    scene.add(Sphere{Point3r{0.0, 0.0, back_wall_z - wall_radius}, wall_radius}, white);
    scene.add(Sphere{Point3r{-room_half_width - wall_radius, 0.0, 0.0}, wall_radius},
              DiffuseMaterial{.albedo = {0.63, 0.07, 0.05}});
    scene.add(Sphere{Point3r{room_half_width + wall_radius, 0.0, 0.0}, wall_radius},
              DiffuseMaterial{.albedo = {0.12, 0.45, 0.15}});
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

// Smaller spheres around the edges: a silver mirror, frosted glass, rough
// copper and matte blue.
void addOuterSpheres(albedo::Scene& scene)
{
    using albedo::ConductorMaterial;
    using albedo::DielectricMaterial;
    using albedo::DiffuseMaterial;
    using albedo::Point3r;
    using albedo::Sphere;
    scene.add(Sphere{Point3r{-1.9, 0.6, 1.3}, 0.6},
              ConductorMaterial{.eta = silver_eta, .k = silver_k, .roughness = 0.0});
    scene.add(Sphere{Point3r{1.8, 0.45, 0.8}, 0.45},
              DielectricMaterial{.ior = 1.5, .roughness = 0.35});
    scene.add(Sphere{Point3r{0.9, 0.35, 2.3}, 0.35},
              ConductorMaterial{.eta = copper_eta, .k = copper_k, .roughness = 0.3});
    scene.add(Sphere{Point3r{-2.2, 0.5, -2.8}, 0.5}, DiffuseMaterial{.albedo = {0.15, 0.3, 0.7}});
}

// A warm light set into the ceiling with only its lowest part showing below it
// (soft shadows and a caustic under the glass), a small cool point light (hard
// shadows) and a faint sky through the open front.
void addLights(albedo::Scene& scene)
{
    scene.addAreaLight(albedo::Sphere{albedo::Point3r{0.0, room_height + 0.6, -1.2}, 1.1},
                       {9.0, 7.6, 5.8}, albedo::DiffuseMaterial{.albedo = {0.0, 0.0, 0.0}});
    scene.addLight(albedo::PointLight{.position = {0.8, 4.2, 3.2}, .intensity = {1.2, 2.0, 3.6}});
    scene.setSky(albedo::SkyLight::uniform({0.05, 0.06, 0.08}));
}

// Where a mesh goes: the middle of the room, clear of the outer spheres, with
// its longest side this long.
constexpr albedo::Point3r mesh_center{0.3, 1.55, -1.4};
constexpr albedo::Real mesh_size{3.0};

// `mesh` without faces that lack area and without positions no remaining face
// uses, so its positions span exactly what can be seen.
albedo::TriangleMesh visiblePart(const albedo::TriangleMesh& mesh)
{
    constexpr std::size_t unused{std::numeric_limits<std::size_t>::max()};
    const std::span<const albedo::Point3r> all_positions{mesh.positions()};
    std::vector<std::size_t> new_index(all_positions.size(), unused);
    std::vector<albedo::Point3r> positions;
    std::vector<albedo::TriangleMesh::Face> faces;
    for (const albedo::TriangleMesh::Face& face : mesh.faces()) {
        if (!albedo::formsTriangle(all_positions[face.at(0)], all_positions[face.at(1)],
                                   all_positions[face.at(2)])) {
            continue;
        }
        albedo::TriangleMesh::Face renumbered{};
        for (std::size_t corner{0}; corner < renumbered.size(); ++corner) {
            std::size_t& index{new_index[face.at(corner)]};
            if (index == unused) {
                index = positions.size();
                positions.push_back(all_positions[face.at(corner)]);
            }
            renumbered.at(corner) = index;
        }
        faces.push_back(renumbered);
    }
    return albedo::TriangleMesh{std::move(positions), std::move(faces)};
}

// The visible part of the mesh at `path`, scaled uniformly and moved so the box
// around it is centred on `center` with its longest side `size` long. Throws
// `std::runtime_error` naming the file if it cannot be read or has nothing to show.
albedo::TriangleMesh fittedMesh(const std::filesystem::path& path, albedo::Point3r center,
                                albedo::Real size)
{
    const auto fail{[&](std::string_view problem) {
        return std::runtime_error{std::format("{}: {}", path.string(), problem)};
    }};
    std::optional<albedo::TriangleMesh> mesh;
    try {
        mesh = visiblePart(albedo::readObj(path));
    } catch (const albedo::ObjParseError& error) {
        throw fail(error.what());
    }
    if (mesh->faces().empty()) {
        throw fail("the mesh has no triangles");
    }
    albedo::Bounds3r box;
    for (const albedo::Point3r& position : mesh->positions()) {
        box = merged(box, position);
    }
    const albedo::Vector3r extent{box.extent()};
    const albedo::Real scale{size / std::max({extent.x, extent.y, extent.z})};
    if (!std::isfinite(scale) || !(scale > 0)) {
        throw fail("the mesh is too large to scale into the room");
    }
    // Halved before adding, so huge coordinates cannot overflow.
    const albedo::Point3r box_center{box.min.x / 2 + box.max.x / 2, box.min.y / 2 + box.max.y / 2,
                                     box.min.z / 2 + box.max.z / 2};
    std::vector<albedo::Point3r> positions;
    positions.reserve(mesh->positions().size());
    for (const albedo::Point3r& position : mesh->positions()) {
        positions.push_back(center + (position - box_center) * scale);
    }
    albedo::TriangleMesh fitted{std::move(positions), {mesh->faces().begin(), mesh->faces().end()}};
    if (fitted.triangles().empty()) {
        throw fail("no triangle of the mesh keeps an area once scaled");
    }
    return fitted;
}

// The showcase room; a mesh, when given, replaces the two central spheres.
albedo::Scene buildScene(const std::optional<std::filesystem::path>& mesh_path)
{
    albedo::Scene scene;
    addRoom(scene);
    if (mesh_path) {
        scene.add(fittedMesh(*mesh_path, mesh_center, mesh_size),
                  albedo::ConductorMaterial{.eta = gold_eta, .k = gold_k, .roughness = 0.15});
    } else {
        addCentralSpheres(scene);
    }
    addOuterSpheres(scene);
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
