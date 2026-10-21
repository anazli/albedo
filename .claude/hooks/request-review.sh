#!/usr/bin/env bash
# Stop (chained from build-and-test.sh once the build and tests pass): if the
# uncommitted C++/CMake change has not been reviewed by cpp-reviewer, block
# stopping and tell Claude to run the review.
set -u

command -v jq >/dev/null 2>&1 || { echo "request-review.sh: jq not found, skipping" >&2; exit 0; }

input=$(cat)

# At most one review request per turn: if Claude is already continuing because
# of a stop hook, let it stop.
if [ "$(jq -r '.stop_hook_active // false' <<<"$input")" = "true" ]; then
  exit 0
fi

cd "${CLAUDE_PROJECT_DIR:-.}" || exit 0
source "$(dirname "$0")/review-lib.sh"

fingerprint=$(current_fingerprint)
[ -z "$fingerprint" ] && exit 0
[ "$fingerprint" = "$(reviewed_fingerprint)" ] && exit 0
review_in_flight_for "$fingerprint" && exit 0

review_instructions >&2
exit 2
