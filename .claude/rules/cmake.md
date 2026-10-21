---
paths:
  - "**/CMakeLists.txt"
  - "**/*.cmake"
---

# CMake rules

- Use target-based commands only (`target_include_directories`, `target_compile_options`, `target_link_libraries`). No `include_directories`, `add_definitions` or `link_libraries`.
- Set the standard per target with `target_compile_features(<target> PUBLIC cxx_std_20)` and keep `CMAKE_CXX_EXTENSIONS` set to `OFF`.
- Use correct visibility: `PRIVATE` by default, `PUBLIC` only when consumers need it, `INTERFACE` for header-only targets.
- List source files explicitly; do not use `file(GLOB ...)`.
- Pin third-party dependencies fetched with `FetchContent` to a tag or commit hash, never a branch.
- Do not change compiler warning or sanitizer settings without being asked.
