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
    geometry/shape_test.cpp
    geometry/sphere_test.cpp
    image/pixel_buffer_test.cpp
    image/ppm_test.cpp
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
add_test(NAME app.rejectsTooManyArguments COMMAND albedo a.ppm b.ppm)
set_tests_properties(app.rejectsTooManyArguments PROPERTIES WILL_FAIL TRUE)
# WILL_FAIL accepts any failure (even a crash), so also check the usage text is printed.
add_test(NAME app.printsUsageOnWrongArguments COMMAND albedo)
set_tests_properties(app.printsUsageOnWrongArguments PROPERTIES
    PASS_REGULAR_EXPRESSION "usage: albedo <output.ppm>")
set(gradient_image ${CMAKE_CURRENT_BINARY_DIR}/gradient.ppm)
add_test(NAME app.removeOldGradientImage COMMAND ${CMAKE_COMMAND} -E rm -f ${gradient_image})
add_test(NAME app.writesGradientImage COMMAND albedo ${gradient_image})
# md5sum fails when the file is missing, so this checks the app actually wrote it.
add_test(NAME app.gradientImageExists COMMAND ${CMAKE_COMMAND} -E md5sum ${gradient_image})
set_tests_properties(app.removeOldGradientImage PROPERTIES FIXTURES_SETUP gradient_cleared)
set_tests_properties(app.writesGradientImage PROPERTIES
    FIXTURES_REQUIRED gradient_cleared
    FIXTURES_SETUP gradient_written)
set_tests_properties(app.gradientImageExists PROPERTIES FIXTURES_REQUIRED gradient_written)
