#!/bin/sh
# check_test_hygiene.sh - fast (<1s) guard against test-list drift.
#
# Fails if:
#   1. any tests/*_test.cpp or tests/*_test.mm is missing from ALL_TESTS
#   2. any test name appears more than once in ALL_TESTS
#   3. any tracked file contains git conflict markers (^(<<<<<<<|>>>>>>>))
#
# Example-catalog makefile enforcement is intentionally NOT here: it is
# owned by the example runner in PR C, so this script passes standalone.
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
MAKEFILE="$SCRIPT_DIR/Makefile"

if [ ! -f "$MAKEFILE" ]; then
  echo "FAIL: tests/Makefile not found at $MAKEFILE" >&2
  exit 1
fi

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT INT TERM

# --- Parse every ALL_TESTS token (:= and +=, including conditional +=) ---
awk '
  function emit(line,   n, i, parts) {
    gsub(/\\/, " ", line)
    n = split(line, parts, /[ \t]+/)
    for (i = 1; i <= n; i++) if (parts[i] != "") print parts[i]
  }
  /^ALL_TESTS[ \t]*[:+]?=/ {
    in_block = 1
    cont = (/\\[ \t]*$/)
    sub(/^ALL_TESTS[ \t]*[:+]?=[ \t]*/, "")
    emit($0)
    if (!cont) in_block = 0
    next
  }
  in_block {
    cont = (/\\[ \t]*$/)
    emit($0)
    if (!cont) in_block = 0
    next
  }
' "$MAKEFILE" > "$tmpdir/all_tests.txt"

if [ ! -s "$tmpdir/all_tests.txt" ]; then
  echo "FAIL: parsed zero ALL_TESTS entries from $MAKEFILE" >&2
  exit 1
fi

sort -u "$tmpdir/all_tests.txt" > "$tmpdir/all_tests_sorted.txt"

# --- Test source files on disk -------------------------------------------
{
  for f in "$SCRIPT_DIR"/*_test.cpp "$SCRIPT_DIR"/*_test.mm; do
    [ -e "$f" ] || continue
    base=${f##*/}
    base=${base%.cpp}
    base=${base%.mm}
    printf '%s\n' "$base"
  done
} > "$tmpdir/file_tests.txt"
sort -u "$tmpdir/file_tests.txt" > "$tmpdir/file_tests_sorted.txt"

fail=0

# --- 1. Orphaned tests -----------------------------------------------------
missing=$(comm -23 "$tmpdir/file_tests_sorted.txt" "$tmpdir/all_tests_sorted.txt" || true)
if [ -n "$missing" ]; then
  echo "FAIL: tests/*_test.{cpp,mm} missing from ALL_TESTS in tests/Makefile:" >&2
  printf '%s\n' "$missing" | sed 's/^/  /' >&2
  fail=1
fi

# --- 2. Duplicate ALL_TESTS entries ---------------------------------------
dupes=$(sort "$tmpdir/all_tests.txt" | uniq -d || true)
if [ -n "$dupes" ]; then
  echo "FAIL: duplicate entries in ALL_TESTS in tests/Makefile:" >&2
  printf '%s\n' "$dupes" | sed 's/^/  /' >&2
  fail=1
fi

# --- 3. Conflict markers in tracked files ---------------------------------
conflicts=""
if git -C "$REPO_ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  # git grep searches tracked files only; exclude this script itself.
  conflicts=$(git -C "$REPO_ROOT" grep -l -E '^(<<<<<<<|>>>>>>>)' -- . ':(exclude)tests/check_test_hygiene.sh' 2>/dev/null || true)
else
  for d in src tests examples; do
    [ -d "$REPO_ROOT/$d" ] || continue
    found=$(find "$REPO_ROOT/$d" -type f ! -name 'check_test_hygiene.sh' -exec grep -l -E '^(<<<<<<<|>>>>>>>)' {} + 2>/dev/null || true)
    if [ -n "$found" ]; then
      # Print repo-relative paths in the fallback path too.
      rel=$(printf '%s\n' "$found" | sed "s|^$REPO_ROOT/||" || true)
      conflicts="${conflicts}${conflicts:+
}$rel"
    fi
  done
fi
if [ -n "$conflicts" ]; then
  echo "FAIL: git conflict markers found in tracked files:" >&2
  printf '%s\n' "$conflicts" | sed 's/^/  /' >&2
  fail=1
fi

if [ "$fail" -ne 0 ]; then
  exit 1
fi

count=$(wc -l < "$tmpdir/all_tests_sorted.txt" | tr -d ' ')
echo "test hygiene ok ($count tests in ALL_TESTS)"
