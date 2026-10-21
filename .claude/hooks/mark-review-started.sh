#!/usr/bin/env bash
# PreToolUse (matcher: Agent|Task): when cpp-reviewer is launched, record the
# change it will review and when, so request-review.sh does not ask for the
# same review again while this one is still running.
set -u

command -v jq >/dev/null 2>&1 || { echo "mark-review-started.sh: jq not found, skipping" >&2; exit 0; }

[ "$(jq -r '.tool_input.subagent_type // empty')" = "cpp-reviewer" ] || exit 0

cd "${CLAUDE_PROJECT_DIR:-.}" || exit 0
source "$(dirname "$0")/review-lib.sh"

fingerprint=$(current_fingerprint)
[ -z "$fingerprint" ] && exit 0
echo "$fingerprint $(date +%s)" >"$(pending_review_file)"
exit 0
