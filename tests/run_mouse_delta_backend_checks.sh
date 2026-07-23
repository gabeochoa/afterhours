#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SMOKE="$ROOT/tests/input_system_mouse_delta_backend_smoke.cpp"
INJECTOR_TEST="$ROOT/tests/input_injector_mouse_delta_test.cpp"
OUT_DIR="$ROOT/output/mouse_delta_checks"
mkdir -p "$OUT_DIR"

echo "==> Running injector delta runtime test"
clang++ -std=c++23 -I"$ROOT/.." "$INJECTOR_TEST" -o "$OUT_DIR/injector_delta_test"
"$OUT_DIR/injector_delta_test"

echo "==> Compile smoke: raylib backend"
clang++ -std=c++23 \
  -I/opt/homebrew/Cellar/raylib/5.5/include \
  -I"$ROOT/vendor" \
  -I"$ROOT/.." \
  -DAFTER_HOURS_ENABLE_E2E_TESTING \
  -DAFTER_HOURS_USE_RAYLIB \
  -c "$SMOKE" \
  -o "$OUT_DIR/smoke_raylib.o"

echo "==> Compile smoke: sokol backend"
clang++ -std=c++23 \
  -I"$ROOT/vendor" \
  -I"$ROOT/.." \
  -DAFTER_HOURS_ENABLE_E2E_TESTING \
  -DAFTER_HOURS_USE_METAL \
  -c "$SMOKE" \
  -o "$OUT_DIR/smoke_sokol.o"

echo "All mouse-delta backend checks passed."
