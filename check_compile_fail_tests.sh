#!/bin/bash

# Compile-fail regression tests.
#
# These tests are expected to FAIL compilation, and we verify the failure
# reason (so we don't accept unrelated compiler/config errors).

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

TEST_FILE="${ROOT_DIR}/compile_fail/snapshot_for_pointer_like_projected_value.cpp"

if [[ ! -f "${TEST_FILE}" ]]; then
  echo "Missing compile-fail test file: ${TEST_FILE}"
  exit 1
fi

ERR_FILE="$(mktemp)"
OUT_FILE="$(mktemp)"
trap 'rm -f "${ERR_FILE}" "${OUT_FILE}"' EXIT

pick_compiler() {
  if command -v clang++ >/dev/null 2>&1; then
    echo "clang++"
    return
  fi
  if command -v g++ >/dev/null 2>&1; then
    echo "g++"
    return
  fi
  echo ""
}

CXX="$(pick_compiler)"
if [[ -z "${CXX}" ]]; then
  echo "No C++ compiler found (need clang++ or g++)"
  exit 1
fi

FLAGS=(-std=c++23 -c)

if [[ "${CXX}" == "clang++" ]]; then
  FLAGS+=(-Wall -Wextra -Wpedantic -Wuninitialized -Wshadow -Wconversion -Wmost)
  # Some environments have clang but mis-detect libstdc++; pin if GCC 13 exists.
  if [[ -d "/usr/lib/gcc/x86_64-linux-gnu/13" ]]; then
    FLAGS+=(--gcc-install-dir=/usr/lib/gcc/x86_64-linux-gnu/13)
  fi
else
  FLAGS+=(-Wall -Wextra -Wpedantic -Wuninitialized -Wshadow -Wconversion)
fi

echo "Running compile-fail tests with ${CXX}..."

if "${CXX}" "${FLAGS[@]}" "${TEST_FILE}" -o "${OUT_FILE}" 2>"${ERR_FILE}"; then
  echo "❌ Expected compilation to fail, but it succeeded:"
  echo "  ${TEST_FILE}"
  exit 1
fi

# Ensure it failed for the right reason (the pointer-free policy).
if ! grep -q "Pointer-like types" "${ERR_FILE}"; then
  echo "❌ Compilation failed, but not due to the pointer-free snapshot policy."
  echo "Compiler output:"
  cat "${ERR_FILE}"
  exit 1
fi

echo "✓ compile-fail snapshot pointer-free regression passed"

