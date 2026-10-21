#!/usr/bin/env bash
# PostToolUse: format a C++ file right after Claude writes or edits it.
# Always exits 0 so a formatting problem never blocks Claude.
set -u

# jq parses the hook input; without it, do nothing rather than guess.
command -v jq >/dev/null 2>&1 || { echo "format-cpp.sh: jq not found, skipping" >&2; exit 0; }

file=$(jq -r '.tool_input.file_path // empty')

case "$file" in
  *.h|*.hpp|*.hxx|*.ipp|*.cpp|*.cc|*.cxx) ;;
  *) exit 0 ;;
esac

[ -f "$file" ] || exit 0
command -v clang-format >/dev/null 2>&1 || exit 0

clang-format -i --style=file "$file" || true
exit 0
