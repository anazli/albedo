include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
# Match the CRT of the project on MSVC instead of GoogleTest's static default.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(albedo_tests
    accel/bvh_test.cpp
    camera/perspective_camera_test.cpp
    color/rgb_test.cpp
    color/srgb_test.cpp
    core/arithmetic_test.cpp
    core/bounds3_test.cpp
    core/interval_test.cpp
    core/normal3_test.cpp
    core/orthonormal_basis_test.cpp
    core/point2_test.cpp
    core/point3_test.cpp
    core/ray_test.cpp
    core/real_test.cpp
    core/transform_test.cpp
    core/vector3_test.cpp
    estimator/normals_estimator_test.cpp
    estimator/path_estimator_test.cpp
    estimator/radiance_estimator_test.cpp
    estimator/scatter_only_estimator_test.cpp
    estimator/surface_ray_test.cpp
    geometry/mesh_geometry_test.cpp
    geometry/mesh_instance_test.cpp
    geometry/shape_test.cpp
    geometry/obj_test.cpp
    geometry/sphere_test.cpp
    geometry/triangle_mesh_test.cpp
    geometry/triangle_test.cpp
    image/pixel_buffer_test.cpp
    image/ppm_test.cpp
    io/line_reader_test.cpp
    io/path_text_test.cpp
    io/tokens_test.cpp
    light/area_light_test.cpp
    light/light_test.cpp
    light/point_light_test.cpp
    light/sky_light_test.cpp
    material/material_test.cpp
    material/mix_material_test.cpp
    render/render_image_test.cpp
    render/tile_test.cpp
    sampling/pcg32_test.cpp
    sampling/sample_generator_test.cpp
    sampling/warps_test.cpp
    scattering/conductor_lobe_test.cpp
    scattering/dielectric_lobe_test.cpp
    scattering/fresnel_test.cpp
    scattering/ggx_distribution_test.cpp
    scattering/lambert_lobe_test.cpp
    scattering/lobe_test.cpp
    scattering/scattering_test.cpp
    scattering/specular_test.cpp
    scene/scene_file_test.cpp
    scene/scene_test.cpp
    texture/checker_texture_test.cpp
    texture/constant_texture_test.cpp
    texture/perlin_noise_test.cpp
    texture/perlin_pattern_test.cpp
    texture/perlin_texture_test.cpp
    texture/texture_test.cpp
)
target_include_directories(albedo_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(albedo_tests PRIVATE albedo_core GTest::gtest_main)
albedo_set_warnings(albedo_tests)
albedo_enable_sanitizers(albedo_tests)

include(GoogleTest)
# Run in the build tree: file-writing tests put their output under it.
gtest_discover_tests(albedo_tests WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# Smoke tests of the command-line app.
set(showcase_scene ${PROJECT_SOURCE_DIR}/assets/showcase.scene)
add_test(NAME app.rejectsMissingArguments COMMAND albedo)
set_tests_properties(app.rejectsMissingArguments PROPERTIES WILL_FAIL TRUE)
add_test(NAME app.rejectsAMissingOutputPath COMMAND albedo ${showcase_scene})
set_tests_properties(app.rejectsAMissingOutputPath PROPERTIES WILL_FAIL TRUE)
add_test(NAME app.rejectsTooManyArguments COMMAND albedo ${showcase_scene} a.ppm 64 extra)
set_tests_properties(app.rejectsTooManyArguments PROPERTIES WILL_FAIL TRUE)
# WILL_FAIL accepts any failure (even a crash), so also check the usage text is printed.
add_test(NAME app.printsUsageOnWrongArguments COMMAND albedo)
set_tests_properties(app.printsUsageOnWrongArguments PROPERTIES
    PASS_REGULAR_EXPRESSION "usage: albedo <scene> <output.ppm> \\[width\\]")
# Each invalid width is checked twice: PASS_REGULAR_EXPRESSION ignores the exit
# code, so a separate WILL_FAIL test checks that the app also fails.
foreach(bad_width IN ITEMS abc 0 15 8193 -64 +64 64px 99999999999)
    add_test(NAME app.rejectsWidth_${bad_width} COMMAND albedo ${showcase_scene} bad.ppm ${bad_width})
    set_tests_properties(app.rejectsWidth_${bad_width} PROPERTIES
        PASS_REGULAR_EXPRESSION "invalid width")
    add_test(NAME app.failsOnWidth_${bad_width} COMMAND albedo ${showcase_scene} bad.ppm ${bad_width})
    set_tests_properties(app.failsOnWidth_${bad_width} PROPERTIES WILL_FAIL TRUE)
endforeach()
add_test(NAME app.rejectsEmptyWidth COMMAND albedo ${showcase_scene} bad.ppm "")
set_tests_properties(app.rejectsEmptyWidth PROPERTIES PASS_REGULAR_EXPRESSION "invalid width")
add_test(NAME app.rejectsSpacePaddedWidth COMMAND albedo ${showcase_scene} bad.ppm " 64")
set_tests_properties(app.rejectsSpacePaddedWidth PROPERTIES
    PASS_REGULAR_EXPRESSION "invalid width")
# Outside the loop because an empty item is dropped and a space breaks the test name.
add_test(NAME app.failsOnEmptyWidth COMMAND albedo ${showcase_scene} bad.ppm "")
set_tests_properties(app.failsOnEmptyWidth PROPERTIES WILL_FAIL TRUE)
add_test(NAME app.failsOnSpacePaddedWidth COMMAND albedo ${showcase_scene} bad.ppm " 64")
set_tests_properties(app.failsOnSpacePaddedWidth PROPERTIES WILL_FAIL TRUE)
# The smallest accepted width; the largest (8192) is not rendered, it would be too slow here.
add_test(NAME app.acceptsMinimumWidth
    COMMAND albedo ${showcase_scene} ${CMAKE_CURRENT_BINARY_DIR}/min_width.ppm 16)

# Renders that must write an image: the app runs between a step that removes
# any old image and one whose md5sum fails if the image is missing.
function(albedo_add_render_test name scene image width)
    add_test(NAME app.removeOld_${name} COMMAND ${CMAKE_COMMAND} -E rm -f ${image})
    add_test(NAME app.${name} COMMAND albedo ${scene} ${image} ${width})
    add_test(NAME app.${name}_imageExists COMMAND ${CMAKE_COMMAND} -E md5sum ${image})
    set_tests_properties(app.removeOld_${name} PROPERTIES FIXTURES_SETUP ${name}_cleared)
    set_tests_properties(app.${name} PROPERTIES
        FIXTURES_REQUIRED ${name}_cleared
        FIXTURES_SETUP ${name}_written)
    set_tests_properties(app.${name}_imageExists PROPERTIES FIXTURES_REQUIRED ${name}_written)
endfunction()
# Small renders keep the smoke tests fast in sanitizer builds.
albedo_add_render_test(rendersTheShowcase ${showcase_scene}
    ${CMAKE_CURRENT_BINARY_DIR}/smoke.ppm 64)
albedo_add_render_test(rendersTheMeshesScene ${PROJECT_SOURCE_DIR}/assets/meshes.scene
    ${CMAKE_CURRENT_BINARY_DIR}/meshes.ppm 16)

# Scenes the app must reject, each checked twice as for the widths above.
add_test(NAME app.failsOnAMissingScene COMMAND albedo no_such.scene bad.ppm 16)
set_tests_properties(app.failsOnAMissingScene PROPERTIES WILL_FAIL TRUE)
add_test(NAME app.reportsAMissingScene COMMAND albedo no_such.scene bad.ppm 16)
set_tests_properties(app.reportsAMissingScene PROPERTIES
    PASS_REGULAR_EXPRESSION "cannot open scene file 'no_such.scene'")
# A directory where a scene file belongs: reported with its path.
set(directory_scene ${CMAKE_CURRENT_BINARY_DIR}/directory_as_scene)
file(MAKE_DIRECTORY ${directory_scene})
add_test(NAME app.failsOnADirectoryAsScene COMMAND albedo ${directory_scene} bad.ppm 16)
set_tests_properties(app.failsOnADirectoryAsScene PROPERTIES WILL_FAIL TRUE)
add_test(NAME app.reportsADirectoryAsScene COMMAND albedo ${directory_scene} bad.ppm 16)
set_tests_properties(app.reportsADirectoryAsScene PROPERTIES
    PASS_REGULAR_EXPRESSION "directory_as_scene' is not a regular file")
# Output naming the scene itself must not destroy it. A throwaway copy, so a
# failing guard cannot damage a real scene; it is restored before the tests and
# compared with the original after them.
set(same_file_scene ${CMAKE_CURRENT_BINARY_DIR}/same_file.scene)
set(same_file_original ${CMAKE_CURRENT_BINARY_DIR}/same_file_original.scene)
file(WRITE ${same_file_original} "camera from 0 0 5 at 0 0 0\n")
add_test(NAME app.restoreSameFileScene
    COMMAND ${CMAKE_COMMAND} -E copy ${same_file_original} ${same_file_scene})
add_test(NAME app.failsWhenTheOutputIsTheScene
    COMMAND albedo ${same_file_scene} ${same_file_scene} 16)
set_tests_properties(app.failsWhenTheOutputIsTheScene PROPERTIES WILL_FAIL TRUE)
add_test(NAME app.reportsWhenTheOutputIsTheScene
    COMMAND albedo ${same_file_scene} ${same_file_scene} 16)
set_tests_properties(app.reportsWhenTheOutputIsTheScene PROPERTIES
    PASS_REGULAR_EXPRESSION "the output would overwrite the scene file")
add_test(NAME app.sameFileSceneIsUntouched
    COMMAND ${CMAKE_COMMAND} -E compare_files ${same_file_original} ${same_file_scene})
set_tests_properties(app.restoreSameFileScene PROPERTIES FIXTURES_SETUP same_file_restored)
set_tests_properties(app.failsWhenTheOutputIsTheScene app.reportsWhenTheOutputIsTheScene
    PROPERTIES FIXTURES_REQUIRED same_file_restored FIXTURES_SETUP same_file_used)
set_tests_properties(app.sameFileSceneIsUntouched PROPERTIES FIXTURES_REQUIRED same_file_used)
set(malformed_scene ${CMAKE_CURRENT_BINARY_DIR}/malformed.scene)
file(WRITE ${malformed_scene} "camera from 0 0 0\n")
add_test(NAME app.failsOnAMalformedScene COMMAND albedo ${malformed_scene} bad.ppm 16)
set_tests_properties(app.failsOnAMalformedScene PROPERTIES WILL_FAIL TRUE)
add_test(NAME app.reportsAMalformedScene COMMAND albedo ${malformed_scene} bad.ppm 16)
set_tests_properties(app.reportsAMalformedScene PROPERTIES
    PASS_REGULAR_EXPRESSION "malformed.scene: scene line 1: missing 'at'")

# Meshes a scene cannot use, each in its own directory beside a scene that
# fits it into view; these were mesh arguments before scene files existed.
# `mesh_text` must be one argument; more would be dropped, so they are refused.
function(albedo_write_mesh_case case mesh_text)
    if(ARGC GREATER 2)
        message(FATAL_ERROR "albedo_write_mesh_case: pass the mesh text as one argument")
    endif()
    set(directory ${CMAKE_CURRENT_BINARY_DIR}/mesh_cases/${case})
    file(MAKE_DIRECTORY ${directory})
    file(WRITE ${directory}/case.scene "camera from 0 0 5 at 0 0 0\n"
                                        "material m diffuse color 0.5 0.5 0.5\n"
                                        "mesh x mesh.obj\n"
                                        "instance x material m fit 2\n")
    if(NOT "${mesh_text}" STREQUAL "")
        file(WRITE ${directory}/mesh.obj "${mesh_text}")
    endif()
endfunction()
albedo_write_mesh_case(missing "")
albedo_write_mesh_case(malformed "v 0 0 0\nf 1 2 3\n")
albedo_write_mesh_case(empty "v 0 0 0\nv 1 0 0\nv 0 1 0\n")
# One face, but its corners are collinear: nothing to show.
albedo_write_mesh_case(degenerate "v 0 0 0\nv 1 0 0\nv 2 0 0\nf 1 2 3\n")
# Two triangles whose combined extent overflows a double.
string(CONCAT huge_mesh_text "v -1e308 0 0\nv -1e308 1 0\nv -1e308 0 1\nf 1 2 3\n"
                             "v 1e308 0 0\nv 1e308 1 0\nv 1e308 0 1\nf 4 5 6\n")
albedo_write_mesh_case(huge "${huge_mesh_text}")
# One unit wide but centred so far out that one transform cannot place it.
albedo_write_mesh_case(far "v 1.7e308 0 0\nv 1.7e308 1 0\nv 1.7e308 0 1\nf 1 2 3\n")
# A sliver with area. Fitted by a transform rather than by rewriting its
# positions, it keeps its area and renders, if invisibly thin.
albedo_write_mesh_case(sliver "v 0 0 0\nv 1e200 0 0\nv 0 1e-100 0\nf 1 2 3\n")
albedo_add_render_test(rendersASliverMesh ${CMAKE_CURRENT_BINARY_DIR}/mesh_cases/sliver/case.scene
    ${CMAKE_CURRENT_BINARY_DIR}/sliver.ppm 16)
foreach(case IN ITEMS missing malformed empty degenerate huge far)
    add_test(NAME app.failsOnMesh_${case}
        COMMAND albedo ${CMAKE_CURRENT_BINARY_DIR}/mesh_cases/${case}/case.scene bad.ppm 16)
    set_tests_properties(app.failsOnMesh_${case} PROPERTIES WILL_FAIL TRUE)
endforeach()
function(albedo_add_mesh_report_test name case pattern)
    add_test(NAME app.${name}
        COMMAND albedo ${CMAKE_CURRENT_BINARY_DIR}/mesh_cases/${case}/case.scene bad.ppm 16)
    set_tests_properties(app.${name} PROPERTIES PASS_REGULAR_EXPRESSION "${pattern}")
endfunction()
albedo_add_mesh_report_test(reportsMissingMesh missing
    "scene line 3: mesh file 'mesh.obj' does not exist")
albedo_add_mesh_report_test(reportsMalformedMesh malformed
    "scene line 3: mesh 'mesh.obj': OBJ line 2")
albedo_add_mesh_report_test(reportsMeshWithoutTriangles empty
    "scene line 3: mesh 'mesh.obj' has no triangles")
albedo_add_mesh_report_test(reportsMeshWithOnlyDegenerateFaces degenerate
    "scene line 3: mesh 'mesh.obj' has no triangles")
albedo_add_mesh_report_test(reportsMeshTooLargeToFit huge
    "scene line 4: the instance is too large or too small to fit")
albedo_add_mesh_report_test(reportsMeshTooFarToPlace far
    "scene line 4: composed transform has a non-finite entry")
