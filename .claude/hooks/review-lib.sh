#!/usr/bin/env bash
# Shared helpers for the cpp-reviewer hooks. Sourced, not executed.
#
# A "fingerprint" identifies the current uncommitted C++/CMake change: a hash
# over the path and working-tree content of every file that differs from HEAD
# (including untracked ones). It ignores the index, so `git add` after a review
# does not invalidate it. The fingerprint of the last change cpp-reviewer saw is
# kept inside .git, so it is per clone and never committed.
#
# While a review is running, a "pending" file holds the fingerprint it was
# launched for and the launch time, so the Stop hook does not ask for a second
# review of the same change. Pending entries expire so a crashed review cannot
# silence the hook forever.

review_paths=('*.h' '*.hpp' '*.hxx' '*.ipp' '*.cpp' '*.cc' '*.cxx' '*CMakeLists.txt' '*.cmake')

# Prints the fingerprint of the current change, or nothing if there is none.
current_fingerprint() {
  local files
  files=$(changed_files)
  [ -z "$files" ] && return 0
  while IFS= read -r file; do
    if [ -f "$file" ]; then
      sha256sum -- "$file"
    else
      echo "deleted $file"
    fi
  done <<<"$files" | sha256sum | cut -d' ' -f1
}

reviewed_fingerprint_file() {
  git rev-parse --git-path claude-reviewed-fingerprint
}

reviewed_fingerprint() {
  cat "$(reviewed_fingerprint_file)" 2>/dev/null || true
}

pending_review_file() {
  git rev-parse --git-path claude-pending-review
}

pending_review_max_age_seconds=1800

# Succeeds when a review of exactly fingerprint $1 was launched recently.
review_in_flight_for() {
  local pending_fingerprint started_at
  read -r pending_fingerprint started_at <"$(pending_review_file)" 2>/dev/null || return 1
  [ "$pending_fingerprint" = "$1" ] || return 1
  [ $(( $(date +%s) - started_at )) -lt "$pending_review_max_age_seconds" ]
}

# Lists the changed files the reviewer should look at.
changed_files() {
  { git diff HEAD --name-only -- "${review_paths[@]}"
    git ls-files --others --exclude-standard -- "${review_paths[@]}"; } 2>/dev/null | sort -u
}

review_instructions() {
  cat <<EOF
Before finishing, launch the cpp-reviewer agent (Agent tool, subagent_type "cpp-reviewer") on the uncommitted C++/CMake change. It cannot run git, so put the output of \`git diff HEAD\` and the paths of new untracked files in its prompt, together with a one-line statement of the change's intent. Changed files:
$(changed_files | sed 's/^/  - /')
Then fix every blocking finding, fix should-fix findings unless you have a concrete reason not to, rerun the build and tests, and report all findings and the verdict to the user.
EOF
}
