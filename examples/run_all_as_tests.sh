#!/bin/bash
# Run every example in this repo as a self-verifying test.
#
#   examples/run_all_as_tests.sh
#
# Catalog examples (examples/catalog/**, one dir per example) run through
# `make -C <dir> test`; top-level examples/ programs are built with
# examples/Makefile and run bounded, from a temp cwd so the PNGs and output
# dirs they write never dirty the working tree.
#
# Every example runs even if others fail. Prints `pass=N fail=M` and exits
# non-zero when anything failed. Knobs:
#   JOBS=N            parallel jobs        (default: CPU count, max 8)
#   EXAMPLE_TIMEOUT=S per-example timeout  (default: 300)

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CATALOG_DIR="$SCRIPT_DIR/catalog"
OUT_DIR="$REPO_ROOT/output"

if command -v sysctl >/dev/null 2>&1; then
    JOBS="${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || echo 4)}"
else
    JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"
fi
[ "$JOBS" -gt 8 ] && JOBS=8
EXAMPLE_TIMEOUT="${EXAMPLE_TIMEOUT:-300}"

TIMEOUT_CMD=""
if command -v timeout >/dev/null 2>&1; then
    TIMEOUT_CMD="timeout $EXAMPLE_TIMEOUT"
elif command -v gtimeout >/dev/null 2>&1; then
    TIMEOUT_CMD="gtimeout $EXAMPLE_TIMEOUT"
fi

WORK="$(mktemp -d "${TMPDIR:-/tmp}/afh-example-tests.XXXXXX")"
mkdir -p "$WORK/logs" "$WORK/status"
echo "logs: $WORK/logs"

# ── Discovery: every catalog dir holding .cpp must have a makefile ──────────
catalog_dirs=()
orphans=()
while IFS= read -r dir; do
    catalog_dirs+=("$dir")
    if [ ! -f "$dir/makefile" ] && [ ! -f "$dir/Makefile" ]; then
        orphans+=("$dir")
    fi
done < <(find "$CATALOG_DIR" -name '*.cpp' -exec dirname {} \; | sort -u)

if [ "${#orphans[@]}" -gt 0 ]; then
    echo "FAIL: catalog directories with .cpp but no makefile:" >&2
    printf '  %s\n' "${orphans[@]}" >&2
    exit 1
fi

slug() { printf '%s' "$1" | tr '/.' '__'; }

run_catalog_job() {
    local dir="$1"
    local name="catalog/${dir#"$CATALOG_DIR"/}"
    local log="$WORK/logs/$(slug "$name").log"
    # shellcheck disable=SC2086
    if $TIMEOUT_CMD make -C "$dir" test >"$log" 2>&1; then
        echo 0 >"$WORK/status/$(slug "$name").status"
    else
        echo $? >"$WORK/status/$(slug "$name").status"
    fi
}

run_top_job() {
    local name="$1" bin="$2"; shift 2
    local log="$WORK/logs/$(slug "examples/$name").log"
    local job_tmp="$WORK/run/$name"
    mkdir -p "$job_tmp"
    (
        echo "+ make -C examples $bin"
        # shellcheck disable=SC2086
        if ! $TIMEOUT_CMD make -C "$SCRIPT_DIR" "$bin"; then
            echo "build failed: $name"
            exit 1
        fi
        if [ "$name" = "profile_harness" ]; then
            echo "build check only"
            exit 0
        fi
        echo "+ (cd $job_tmp && $OUT_DIR/$bin $*)"
        cd "$job_tmp" || exit 1
        # shellcheck disable=SC2086
        $TIMEOUT_CMD "$OUT_DIR/$bin" "$@"
    ) >"$log" 2>&1
    echo $? >"$WORK/status/$(slug "examples/$name").status"
}
export WORK SCRIPT_DIR CATALOG_DIR OUT_DIR TIMEOUT_CMD
export -f slug run_catalog_job run_top_job

# ── Run everything in parallel, collecting all failures ─────────────────────
printf '%s\n' "${catalog_dirs[@]}" |
    xargs -P "$JOBS" -I {} bash -c 'run_catalog_job "$@"' _ {}

top_jobs=(
    "text_input_placeholder"
    "imm_visual_check"
    "on_draw_example"
    "drawing_3d_example"
    "graphics_example"
    "headless_validation"
    "perf_stress_test"
    "calendar_stress_test"
    "profile_harness"
)
printf '%s\n' "${top_jobs[@]}" |
    xargs -P "$JOBS" -I {} bash -c '
        case "$1" in
            perf_stress_test)   run_top_job "$1" "$1" entity ;;
            calendar_stress_test) run_top_job "$1" "$1" --layout-only --frames=5 --meetings=10 ;;
            headless_validation)  run_top_job "$1" "$1" --output-dir="$WORK/run/headless_validation/out" ;;
            *)                  run_top_job "$1" "$1" ;;
        esac' _ {}

# ── Summary ──────────────────────────────────────────────────────────────────
pass=0
fail=0
failed=()
for status_file in "$WORK"/status/*.status; do
    [ -e "$status_file" ] || continue
    name="$(basename "$status_file" .status)"
    if [ "$(cat "$status_file")" = "0" ]; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
        failed+=("$name")
    fi
done

echo
echo "pass=$pass fail=$fail"
if [ "$fail" -gt 0 ]; then
    echo "failed:"
    printf '  %s\n' "${failed[@]}" | sort
    for name in "${failed[@]}"; do
        echo "--- tail of $WORK/logs/$name.log"
        tail -n 15 "$WORK/logs/$name.log" 2>/dev/null
    done
    exit 1
fi
exit 0
