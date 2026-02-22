
#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=== Afterhours Examples Test Runner ==="
echo "Running all example directories..."
echo

# Category directories (these contain examples, but are not examples themselves)
CATEGORY_DIRS="core|memory|utility|plugins|ui|safety|shared"

# Track results
success_count=0
fail_count=0
total_examples=0
failed_examples=()

run_example() {
    local dir="$1"
    local label="$2"
    local make_target="${3:-}"
    ((total_examples++))
    echo "=== Running $label ==="
    if [ -n "$make_target" ]; then
        echo "  Running make $make_target..."
        if make -C "$dir" "$make_target" 2>&1; then
            echo "SUCCESS: $label"
            ((success_count++))
        else
            echo "FAILED: $label"
            ((fail_count++))
            failed_examples+=("$label")
        fi
    else
        echo "  Running make..."
        if make -C "$dir" 2>&1; then
            echo "SUCCESS: $label"
            ((success_count++))
        else
            echo "FAILED: $label"
            ((fail_count++))
            failed_examples+=("$label")
        fi
    fi
    echo
}

# ── Part 1: examples/ directory (autolayout_test, drawing_3d_example, graphics_example) ──

EXAMPLES_DIR="$REPO_ROOT/examples"
if [ -d "$EXAMPLES_DIR" ] && [ -f "$EXAMPLES_DIR/Makefile" ]; then
    echo "--- examples/ (raylib & autolayout tests) ---"
    echo

    # Build all three
    run_example "$EXAMPLES_DIR" "examples/build_autolayout_test" "autolayout_test"
    run_example "$EXAMPLES_DIR" "examples/build_drawing_3d_example" "drawing_3d_example"
    run_example "$EXAMPLES_DIR" "examples/build_graphics_example" "graphics_example"

    # Run all three
    for bin in autolayout_test drawing_3d_example graphics_example; do
        ((total_examples++))
        echo "=== Running examples/$bin ==="
        if "$EXAMPLES_DIR/$bin" 2>&1; then
            echo "SUCCESS: examples/$bin"
            ((success_count++))
        else
            echo "FAILED: examples/$bin"
            ((fail_count++))
            failed_examples+=("examples/$bin")
        fi
        echo
    done
fi

# ── Part 2: example/ directory (unit tests & plugin examples) ──

echo "--- example/ (unit tests & plugin examples) ---"
echo

cd "$SCRIPT_DIR"

# Find all directories with makefiles recursively (excluding category directories themselves)
all_dirs=$(find . -type f \( -name "makefile" -o -name "Makefile" \) | while read makefile; do
    dir=$(dirname "$makefile")
    # Skip the root directory (.)
    if [ "$dir" = "." ]; then
        continue
    fi
    # Skip if the directory is a category directory at root level
    base=$(basename "$dir")
    parent=$(dirname "$dir")
    if [ "$parent" = "." ] && echo "$base" | grep -qE "^($CATEGORY_DIRS)$"; then
        continue
    fi
    echo "$dir"
done | sort -u)

if [ -n "$all_dirs" ]; then
    for dir in $all_dirs; do
        example_name=$(echo "$dir" | sed 's|^\./||')
        run_example "$dir" "example/$example_name"
    done
fi

# Summary
echo "=== Summary ==="
echo "Total: $total_examples"
echo "Passed: $success_count"
echo "Failed: $fail_count"

if [ $fail_count -eq 0 ]; then
    echo "All examples ran successfully!"
    exit 0
else
    echo "Some examples failed:"
    for example in "${failed_examples[@]}"; do
        echo "  - $example"
    done
    exit 1
fi
