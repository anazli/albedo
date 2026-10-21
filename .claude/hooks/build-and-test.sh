#!/usr/bin/env bash
# Stop: when C++ or CMake files have uncommitted changes, build and run the
# tests under ASan/UBSan. On failure, exit 2 so Claude keeps working and sees
# the tail of the output on stderr.
set -u

# jq parses the hook input; without it, do nothing rather than guess.
command -v jq >/dev/null 2>&1 || { echo "build-and-test.sh: jq not found, skipping" >&2; exit 0; }

input=$(cat)

# Prevent an endless loop: if Claude is already continuing because of this
# hook, let it stop and leave the remaining failure to the user.
if [ "$(jq -r '.stop_hook_active // false' <<<"$input")" = "true" ]; then
  exit 0
fi

cd "${CLAUDE_PROJECT_DIR:-.}" || exit 0

changed=$(git status --porcelain --untracked-files=all 2>/dev/null \
  | awk '{print $NF}' \
  | grep -E '\.(h|hpp|hxx|ipp|cpp|cc|cxx)$|CMakeLists\.txt$|\.cmake$' || true)

# Nothing C++-related changed (e.g. Claude only answered a question): skip.
[ -z "$changed" ] && exit 0

build_dir="build-asan"   # same directory the /verify skill uses

if [ ! -d "$build_dir" ]; then
  if ! out=$(cmake -S . -B "$build_dir" \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" 2>&1); then
    { echo "CMake configure failed:"; tail -n 40 <<<"$out"; } >&2
    exit 2
  fi
fi

if ! out=$(cmake --build "$build_dir" -j 2>&1); then
  { echo "Build failed. Fix the errors before finishing:"; tail -n 60 <<<"$out"; } >&2
  exit 2
fi

if ! out=$(ctest --test-dir "$build_dir" --output-on-failure 2>&1); then
  { echo "Tests failed. Fix them before finishing (do not disable or weaken tests):"; tail -n 80 <<<"$out"; } >&2
  exit 2
fi

# Build and tests are green: hand over to the review gate. Chained rather than
# registered separately because hooks on the same event run in parallel.
exec "$(dirname "$0")/request-review.sh" <<<"$input"
