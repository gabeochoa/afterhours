#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SMOKE="$ROOT/tests/input_system_mouse_delta_backend_smoke.cpp"
INJECTOR_TEST="$ROOT/tests/input_injector_mouse_delta_test.cpp"
OUT_DIR="$ROOT/output/mouse_delta_checks"
mkdir -p "$OUT_DIR"

# Pin <afterhours/...> to THIS checkout regardless of the directory's name.
# A bare `-I "$ROOT/.."` resolves <afterhours/...> to the *parent* of the
# checkout dir, which only works when the checkout is literally named
# `afterhours`. In a git worktree (or any differently-named clone) that
# silently picks up a *sibling* `afterhours` checkout. Generate a private
# include dir containing a symlink named exactly `afterhours` -> this checkout
# root, and use it as the sole afterhours root. (Mirrors tests/ + examples/
# Makefiles.)
INCLUDE_ROOT="$OUT_DIR/.afh-include"
mkdir -p "$INCLUDE_ROOT"
ln -sfn "$ROOT" "$INCLUDE_ROOT/afterhours"

# Homebrew prefix differs per machine (/opt/homebrew on Apple-Silicon default,
# ~/homebrew for a per-user install). Auto-detect via `brew --prefix raylib`
# and fall back to the classic Cellar path. Override via RAYLIB_PREFIX env.
RAYLIB_PREFIX="${RAYLIB_PREFIX:-$(brew --prefix raylib 2>/dev/null)}"
RAYLIB_PREFIX="${RAYLIB_PREFIX:-/opt/homebrew/Cellar/raylib/5.5}"

echo "==> Running injector delta runtime test"
clang++ -std=c++23 -isystem "$INCLUDE_ROOT" "$INJECTOR_TEST" -o "$OUT_DIR/injector_delta_test"
"$OUT_DIR/injector_delta_test"

echo "==> Compile smoke: raylib backend"
clang++ -std=c++23 \
  -I"$RAYLIB_PREFIX/include" \
  -isystem "$INCLUDE_ROOT" \
  -isystem "$ROOT/vendor" \
  -DAFTER_HOURS_ENABLE_E2E_TESTING \
  -DAFTER_HOURS_USE_RAYLIB \
  -c "$SMOKE" \
  -o "$OUT_DIR/smoke_raylib.o"

echo "==> Compile smoke: sokol backend"
clang++ -std=c++23 \
  -isystem "$INCLUDE_ROOT" \
  -isystem "$ROOT/vendor" \
  -DAFTER_HOURS_ENABLE_E2E_TESTING \
  -DAFTER_HOURS_USE_METAL \
  -c "$SMOKE" \
  -o "$OUT_DIR/smoke_sokol.o"

echo "All mouse-delta backend checks passed."
