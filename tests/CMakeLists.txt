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
    core/normal3_test.cpp
    core/point2_test.cpp
    core/point3_test.cpp
    core/real_test.cpp
    core/vector3_test.cpp
)
target_include_directories(albedo_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(albedo_tests PRIVATE albedo_core GTest::gtest_main)
albedo_set_warnings(albedo_tests)
albedo_enable_sanitizers(albedo_tests)

include(GoogleTest)
gtest_discover_tests(albedo_tests)
