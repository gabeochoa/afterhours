#!/bin/sh
# Theme-bypass lint: no hardcoded Color{...} literals in widget code. A literal
# means the widget invents its own color instead of taking one from Theme
# (with_background(Theme::Usage::...) or a theme field). Files with legitimate
# literals (semantic severity, computed contrast, caller-supplied defaults,
# debug overlays) are allowlisted in theme_bypass_allowlist.txt with the exact
# expected hit count, so a new literal still fails. theme.h / theme_io.h /
# component_config.h define and plumb colors and are exempt. Line comments are
# stripped first, so doc examples do not flag.
# Usage: sh theme_bypass_check.sh [--self-test]
set -eu
cd "$(dirname "$0")/.."
SCAN_DIR="src/plugins/ui"
ALLOWLIST="tests/theme_bypass_allowlist.txt"
EXEMPT=" src/plugins/ui/theme.h src/plugins/ui/theme_io.h src/plugins/ui/component_config.h "

is_exempt() { case "$EXEMPT" in *" $1 "*) return 0 ;; *) return 1 ;; esac; }

# Prints "count path" rows for every scanned file with at least one hit.
scan() {
  for f in $(find "$SCAN_DIR" -type f \( -name '*.h' -o -name '*.cpp' \) | sort); do
    path="src/plugins/ui/${f#$SCAN_DIR/}"
    if is_exempt "$path"; then continue; fi
    n=$(sed 's://.*$::' "$f" | grep -o 'Color{[0-9]' | wc -l | tr -d ' ')
    if [ "$n" -gt 0 ]; then echo "$n $path"; fi
  done
}

check() {
  hits=$(scan)
  # Non-allowlisted hits fail.
  ok=1
  while IFS= read -r row; do
    [ -z "$row" ] && continue
    cnt=${row%% *}; path=${row#* }
    if ! grep -q "^$path $cnt " "$ALLOWLIST" 2>/dev/null; then
      echo "theme-bypass: $path has $cnt hardcoded Color{...} literal(s) — use Theme::Usage / a theme field, or allowlist with a reason" >&2
      grep -n 'Color{[0-9]' "$path" | sed 's/^/    /' >&2 || true
      ok=0
    fi
  done <<EOF
$hits
EOF
  # Allowlist entries that no longer match fail, so the file cannot go stale.
  while IFS= read -r line; do
    case "$line" in ''|'#'*) continue ;; esac
    path=${line%% *}; want=$(echo "$line" | awk '{print $2}')
    got=$(echo "$hits" | awk -v p="$path" '$2==p {print $1}'); got=${got:-0}
    if [ "$got" != "$want" ]; then
      echo "theme-bypass: stale allowlist entry for $path (want $want, found $got) — update tests/theme_bypass_allowlist.txt" >&2
      ok=0
    fi
  done < "$ALLOWLIST"
  if [ "$ok" -ne 1 ]; then exit 1; fi
  echo "theme-bypass ok ($(echo "$hits" | grep -c . || true) allowlisted file(s))"
}

self_test() {
  tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
  mkdir -p "$tmp/src/plugins/ui"
  SCAN_DIR="$tmp/src/plugins/ui"
  ALLOWLIST="$tmp/allowlist.txt"; : > "$ALLOWLIST"
  printf 'void f() { Color c = Color{9, 9, 9, 255}; }\n' > "$SCAN_DIR/widget.h"
  if ( check ) 2>/dev/null; then echo "self-test FAILED: planted bypass passed" >&2; exit 1; fi
  echo "self-test: planted bypass caught"
  printf 'src/plugins/ui/widget.h 1 planted self-test literal\n' > "$ALLOWLIST"
  ( check ) >/dev/null || { echo "self-test FAILED: allowlisted bypass did not pass" >&2; exit 1; }
  echo "self-test: allowlisted bypass passes"
  printf 'void f() {}\n' > "$SCAN_DIR/widget.h"
  if ( check ) 2>/dev/null; then echo "self-test FAILED: stale allowlist entry passed" >&2; exit 1; fi
  echo "self-test: stale allowlist entry caught"
  rm -f "$SCAN_DIR/widget.h"; : > "$ALLOWLIST"
  ( check ) >/dev/null || { echo "self-test FAILED: clean tree did not pass" >&2; exit 1; }
  echo "self-test ok"
}

case "${1:-}" in
  --self-test) self_test ;;
  *) check ;;
esac
