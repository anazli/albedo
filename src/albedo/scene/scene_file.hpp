#pragma once

#include "albedo/camera/perspective_camera.hpp"
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
    /// Most spheres, area lights and point lights together. Adding an object
    /// rebuilds the scene's hierarchy, so loading time grows with the square of
    /// this; large geometry belongs in meshes.
    std::size_t max_objects{4096};
    /// Most `material` statements accepted.
    std::size_t max_materials{4096};
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
///     material NAME conductor eta R G B  k R G B  [roughness X]
///     material NAME dielectric ior X  [roughness X]
///     sphere X Y Z  radius R  material NAME
///     point_light X Y Z  intensity R G B
///     area_light X Y Z  radius R  radiance R G B  [material NAME]
///
/// `up` defaults to +y and `fov` to 60 degrees; roughness defaults to 0; an area
/// light without a material is black. Exactly one camera is required, and at
/// most one sky is allowed (black if absent). A material must be defined before
/// it is used. Numbers are as `parseReal` reads them, and must lie in ranges
/// that keep rendering well defined: reflectances (`color`, `even`, `odd`) in
/// [0, 1] per channel; emitted light (`sky`, `intensity`, `radiance`) in
/// [0, 1e6]; `roughness` in [0, 1]; `ior` in [0.01, 100]; conductor `eta` in
/// [0.001, 1000] and `k` in [0, 1000], with no channel having both eta 1 and k
/// 0; `frequency` positive. Comments run from `#` to the end of a line; blank
/// lines are ignored. Objects and lights are added to the scene in file order.
/// Error messages quote at most 32 bytes of the input, with bytes outside
/// printable ASCII escaped.
///
/// Throws `SceneParseError` for anything else: unknown statements or fields,
/// missing or repeated fields, malformed or out-of-range values (including
/// those the camera, sphere and material types reject), undefined or repeated
/// names, and input exceeding `limits`. Throws what `LineReader` throws for
/// an unreadable stream or an unusable line limit, and `std::bad_alloc` if
/// memory runs out.
[[nodiscard]] SceneDescription readScene(std::istream& input, const SceneLimits& limits = {});

/// Reads the scene file at `path` as `readScene(std::istream&, ...)` does. Throws
/// as that does, and `std::runtime_error` naming the path if the file cannot be
/// opened.
[[nodiscard]] SceneDescription readScene(const std::filesystem::path& path,
                                         const SceneLimits& limits = {});

} // namespace albedo
