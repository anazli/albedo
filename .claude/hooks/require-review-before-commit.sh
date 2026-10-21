#!/usr/bin/env bash
# PreToolUse (matcher: Bash): refuse `git commit` while the uncommitted
# C++/CMake change differs from what cpp-reviewer last reviewed. Exit 2 blocks
# the command and shows stderr to Claude.
set -u

command -v jq >/dev/null 2>&1 || { echo "require-review-before-commit.sh: jq not found, skipping" >&2; exit 0; }

command=$(jq -r '.tool_input.command // empty')

# Only gate commits; matches `git commit` and `git -C <dir> commit`.
grep -Eq '(^|[;&|[:space:]])git([[:space:]]+-C[[:space:]]+[^[:space:]]+)?[[:space:]]+commit([[:space:]]|$)' \
  <<<"$command" || exit 0

cd "${CLAUDE_PROJECT_DIR:-.}" || exit 0
source "$(dirname "$0")/review-lib.sh"

fingerprint=$(current_fingerprint)
[ -z "$fingerprint" ] && exit 0
[ "$fingerprint" = "$(reviewed_fingerprint)" ] && exit 0

{ echo "Commit blocked: the C++/CMake change has not been reviewed in its current form."
  review_instructions; } >&2
exit 2
