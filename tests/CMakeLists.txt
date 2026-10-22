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
    core/vector3_test.cpp
    estimator/normals_estimator_test.cpp
    estimator/path_estimator_test.cpp
    estimator/radiance_estimator_test.cpp
    estimator/scatter_only_estimator_test.cpp
    estimator/surface_ray_test.cpp
    geometry/shape_test.cpp
    geometry/sphere_test.cpp
    image/pixel_buffer_test.cpp
    image/ppm_test.cpp
    light/area_light_test.cpp
    light/light_test.cpp
    light/point_light_test.cpp
    light/sky_light_test.cpp
    material/material_test.cpp
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
    scene/scene_test.cpp
)
target_include_directories(albedo_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(albedo_tests PRIVATE albedo_core GTest::gtest_main)
albedo_set_warnings(albedo_tests)
albedo_enable_sanitizers(albedo_tests)

include(GoogleTest)
# Run in the build tree: file-writing tests put their output under it.
gtest_discover_tests(albedo_tests WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# Smoke tests of the command-line app.
add_test(NAME app.rejectsMissingOutputPath COMMAND albedo)
set_tests_properties(app.rejectsMissingOutputPath PROPERTIES WILL_FAIL TRUE)
add_test(NAME app.rejectsTooManyArguments COMMAND albedo a.ppm 64 extra)
set_tests_properties(app.rejectsTooManyArguments PROPERTIES WILL_FAIL TRUE)
# WILL_FAIL accepts any failure (even a crash), so also check the usage text is printed.
add_test(NAME app.printsUsageOnWrongArguments COMMAND albedo)
set_tests_properties(app.printsUsageOnWrongArguments PROPERTIES
    PASS_REGULAR_EXPRESSION "usage: albedo <output.ppm> \\[width\\]")
# Each invalid width is checked twice: PASS_REGULAR_EXPRESSION ignores the exit
# code, so a separate WILL_FAIL test checks that the app also fails.
foreach(bad_width IN ITEMS abc 0 15 8193 -64 +64 64px 99999999999)
    add_test(NAME app.rejectsWidth_${bad_width} COMMAND albedo bad.ppm ${bad_width})
    set_tests_properties(app.rejectsWidth_${bad_width} PROPERTIES
        PASS_REGULAR_EXPRESSION "invalid width")
    add_test(NAME app.failsOnWidth_${bad_width} COMMAND albedo bad.ppm ${bad_width})
    set_tests_properties(app.failsOnWidth_${bad_width} PROPERTIES WILL_FAIL TRUE)
endforeach()
add_test(NAME app.rejectsEmptyWidth COMMAND albedo bad.ppm "")
set_tests_properties(app.rejectsEmptyWidth PROPERTIES PASS_REGULAR_EXPRESSION "invalid width")
add_test(NAME app.rejectsSpacePaddedWidth COMMAND albedo bad.ppm " 64")
set_tests_properties(app.rejectsSpacePaddedWidth PROPERTIES
    PASS_REGULAR_EXPRESSION "invalid width")
# Outside the loop because an empty item is dropped and a space breaks the test name.
add_test(NAME app.failsOnEmptyWidth COMMAND albedo bad.ppm "")
set_tests_properties(app.failsOnEmptyWidth PROPERTIES WILL_FAIL TRUE)
add_test(NAME app.failsOnSpacePaddedWidth COMMAND albedo bad.ppm " 64")
set_tests_properties(app.failsOnSpacePaddedWidth PROPERTIES WILL_FAIL TRUE)
# The smallest accepted width; the largest (8192) is not rendered, it would be too slow here.
add_test(NAME app.acceptsMinimumWidth COMMAND albedo ${CMAKE_CURRENT_BINARY_DIR}/min_width.ppm 16)

# A small render keeps the smoke test fast in sanitizer builds.
set(smoke_image ${CMAKE_CURRENT_BINARY_DIR}/smoke.ppm)
add_test(NAME app.removeOldSmokeImage COMMAND ${CMAKE_COMMAND} -E rm -f ${smoke_image})
add_test(NAME app.writesImage COMMAND albedo ${smoke_image} 64)
# md5sum fails when the file is missing, so this checks the app actually wrote it.
add_test(NAME app.smokeImageExists COMMAND ${CMAKE_COMMAND} -E md5sum ${smoke_image})
set_tests_properties(app.removeOldSmokeImage PROPERTIES FIXTURES_SETUP smoke_cleared)
set_tests_properties(app.writesImage PROPERTIES
    FIXTURES_REQUIRED smoke_cleared
    FIXTURES_SETUP smoke_written)
set_tests_properties(app.smokeImageExists PROPERTIES FIXTURES_REQUIRED smoke_written)
