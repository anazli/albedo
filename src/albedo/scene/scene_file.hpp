#pragma once

#include "albedo/camera/perspective_camera.hpp"
#include "albedo/geometry/obj.hpp"
#include "albedo/scene/scene.hpp"

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <stdexcept>
#include <string_view>

namespace albedo {

/// Bounds on what `readScene` accepts, so a hostile or corrupt file cannot make
/// it exhaust memory or time.
struct SceneLimits {
    /// Longest line accepted, in bytes, not counting the line terminator.
    std::size_t max_line_length{4096};
    /// Most spheres, area lights, point lights and instances together. Adding an
    /// object rebuilds the scene's hierarchy, so loading time grows with the
    /// square of this; large geometry belongs in meshes.
    std::size_t max_objects{4096};
    /// Most `material` statements accepted.
    std::size_t max_materials{4096};
    /// Most `mesh` statements accepted.
    std::size_t max_meshes{16};
    /// Bounds on the OBJ files that `mesh` statements read, for all of them
    /// together rather than each: their vertices, texture coordinates and
    /// triangles add up against these totals. `max_line_length` applies per line.
    ObjLimits mesh_limits{};
    /// Most operations one `instance` statement may list.
    std::size_t max_instance_operations{16};
    /// Most mesh points that all `ground` operations in the scene may examine
    /// together; each `ground` examines every point its mesh's triangles use.
    std::size_t max_ground_points{std::size_t{1} << 26U};
};

/// What `readScene` throws for input it cannot accept, naming the line at fault.
class SceneParseError : public std::runtime_error {
public:
    /// `problem` describes what is wrong; the message adds the line number.
    /// `line` is 0 for a problem with the file as a whole, such as a missing camera.
    SceneParseError(std::size_t line, std::string_view problem);

    /// The 1-based number of the offending line, or 0 for the whole file.
    [[nodiscard]] std::size_t line() const noexcept { return line_number; }

private:
    std::size_t line_number;
};

/// A scene read from a file, with the camera that looks at it.
struct SceneDescription {
    /// Everything a ray can hit, and the lights.
    Scene scene;
    /// Where the camera is and where it looks. The aspect ratio is left at its
    /// default for the caller to set from the image size.
    CameraSettings camera;
};

/// Reads a scene: one statement per line, a keyword followed by fields. A
/// field is a name followed by its values; fields may come in any order, each
/// at most once, and those in brackets below are optional. Names are
/// identifiers: a letter or `_`, then letters, digits, `_` or `-`.
///
///     camera from X Y Z  at X Y Z  [up X Y Z]  [fov DEGREES]
///     sky R G B
///     sky nadir R G B  zenith R G B
///     material NAME diffuse color R G B
///     material NAME checker even R G B  odd R G B  frequency F
///     material NAME perlin low R G B  high R G B  scale S  [veins V]
///     material NAME mix first NAME  second NAME  scale S  [veins V]  [threshold T]
///     material NAME conductor eta R G B  k R G B  [roughness X]
///     material NAME dielectric ior X  [roughness X]
///     sphere X Y Z  radius R  material NAME
///     point_light X Y Z  intensity R G B
///     area_light X Y Z  radius R  radiance R G B  [material NAME]
///     mesh NAME PATH
///     instance MESH material NAME  [OPERATION...]
///
/// `up` defaults to +y and `fov` to 60 degrees; roughness defaults to 0; an
/// area light without a material is black; `veins` defaults to 0 (see
/// `PerlinPattern`); `threshold` defaults to 0.5 (see `MixMaterial`), and a
/// mix's materials must be defined and not mixes. Exactly one camera is
/// required, and at most one sky is allowed (black if absent). A material must
/// be defined before it is used. Numbers are as `parseReal` reads them, and
/// must lie in ranges that keep rendering well defined: reflectances (`color`,
/// `even`, `odd`, `low`, `high`) in [0, 1] per channel; emitted light (`sky`,
/// `intensity`, `radiance`) in [0, 1e6]; `roughness` in [0, 1]; `ior` in
/// [0.01, 100]; conductor `eta` in [0.001, 1000] and `k` in [0, 1000], with no
/// channel having both eta 1 and k 0; `frequency` and `scale` positive; `veins`
/// in [0, 100]; `threshold` in [0, 1]. Comments run from `#` to the end of a line;
/// blank lines are ignored. Objects and lights are added to the scene in file
/// order. Error messages quote at most 32 bytes of the input, with bytes
/// outside printable ASCII escaped.
///
/// `mesh` reads the Wavefront OBJ file at PATH (see `readObj`), resolved against
/// `base_directory`; an empty `base_directory` means the current directory, and
/// it must exist. PATH must be UTF-8 without spaces or control characters,
/// relative, without `..`, and without parts that are device names on Windows
/// (such as `nul` or `com1.obj`, rejected on every platform); it must name a
/// regular file still inside `base_directory` once symbolic links are resolved.
/// So a scene can only read meshes kept beside it, though the check cannot stop
/// the directory being changed between the check and the opening of the file.
/// A mesh needs at least one triangle with area. `instance` places a mesh, which any
/// number of instances share, with a material and up to
/// `limits.max_instance_operations` operations applied in the order written:
///
///     translate X Y Z          move by (X, Y, Z)
///     scale S | scale X Y Z    scale about the origin, uniformly or per axis
///     rotate AX AY AZ DEGREES  turn about an axis through the origin
///     fit SIZE                 centre the bounding box of the instance as
///                              placed so far on the origin and scale it so
///                              that box's longest side is SIZE (after a
///                              rotation, the box is looser than the mesh)
///     ground                   lift it so its lowest point is exactly at y = 0
///
/// Throws as above, and `SceneParseError` too for a mesh path breaking those
/// rules, a mesh file that cannot be opened or read (naming the OBJ line), a
/// mesh without triangles, meshes or `ground` operations exceeding `limits`,
/// and operations that fail or take the instance out of range. Throws
/// `std::invalid_argument` for an unusable `limits.mesh_limits.max_line_length`.
///
/// Throws `SceneParseError` for anything else: unknown statements or fields,
/// missing or repeated fields, malformed or out-of-range values (including
/// those the camera, sphere and material types reject), undefined or repeated
/// names, and input exceeding `limits`. Throws what `LineReader` throws for
/// an unreadable stream or an unusable line limit, and `std::bad_alloc` if
/// memory runs out.
[[nodiscard]] SceneDescription readScene(std::istream& input,
                                         const std::filesystem::path& base_directory,
                                         const SceneLimits& limits = {});

/// Reads a scene as above, without a directory to find meshes in: a `mesh`
/// statement is a `SceneParseError`.
[[nodiscard]] SceneDescription readScene(std::istream& input, const SceneLimits& limits = {});

/// Reads the scene file at `path` as `readScene(std::istream&, ...)` does, finding
/// meshes in the file's own directory. Throws as that does, and
/// `std::runtime_error` naming the path if it is not a regular file or cannot be
/// opened.
[[nodiscard]] SceneDescription readScene(const std::filesystem::path& path,
                                         const SceneLimits& limits = {});

} // namespace albedo
