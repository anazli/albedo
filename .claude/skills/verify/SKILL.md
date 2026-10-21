---
name: verify
description: Build the project, run the full test suite under AddressSanitizer and UndefinedBehaviorSanitizer, and check formatting and clang-tidy on changed C++ files. Use before reporting a change as done or before committing.
allowed-tools: Bash(cmake *) Bash(ctest *) Bash(clang-format *) Bash(clang-tidy *) Bash(git status *) Bash(git diff *)
---

# Verify

## Changed files

!`git status --short`

## Steps

Run every step, even if an earlier one fails, so the report is complete.

1. Configure the sanitizer build if `build-asan/` does not exist:
   `cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"`
   Add `-DCMAKE_CXX_COMPILER=<compiler>` when the default compiler lacks `std::format` (GCC < 13).
2. Build: `cmake --build build-asan -j`. Warnings are errors; treat any warning as a failure.
3. Test: `ctest --test-dir build-asan --output-on-failure`.
4. Format: `clang-format --dry-run --Werror` on the changed `.h`, `.hpp`, `.cpp` files listed above.
5. Lint: `clang-tidy -p build-asan` on the changed `.cpp` files listed above.

## Report

Give one line per step: pass or fail, with the first relevant error for failures.

If a failure is caused by the current change, fix it and rerun the failed steps. If it predates the change, report it and do not fix it unasked. Never disable a test, add a sanitizer suppression, add `NOLINT`, or lower warning levels to get a pass.
