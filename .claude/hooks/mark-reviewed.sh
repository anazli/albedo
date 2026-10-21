#!/usr/bin/env bash
# SubagentStop (matcher: cpp-reviewer): record the fingerprint of the change
# the reviewer just looked at, so the Stop and commit gates know it is reviewed.
# Any later edit changes the fingerprint and requires a new review.
set -u

cat >/dev/null  # the hook input is not needed

cd "${CLAUDE_PROJECT_DIR:-.}" || exit 0
source "$(dirname "$0")/review-lib.sh"

current_fingerprint >"$(reviewed_fingerprint_file)"
rm -f "$(pending_review_file)"
exit 0
