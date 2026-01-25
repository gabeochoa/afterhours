
#!/bin/bash

echo "=== Afterhours Examples Test Runner ==="
echo "Running all example directories..."
echo

# Category directories (these contain examples, but are not examples themselves)
CATEGORY_DIRS="core|memory|utility|plugins|ui|safety|shared"

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

if [ -z "$all_dirs" ]; then
    echo "No directories with makefiles found"
    exit 1
fi

# Count total examples
total_examples=$(echo "$all_dirs" | wc -l | tr -d ' ')
echo "Found $total_examples example(s) to run:"
echo "$all_dirs" | sed 's|^\./||' | sed 's/^/  - /'
echo

# Track results
success_count=0
fail_count=0
failed_examples=()

# Run each example
for dir in $all_dirs; do
    example_name=$(echo "$dir" | sed 's|^\./||')
    echo "=== Running $example_name ==="

    echo "  Found makefile, running make..."
    if make -C "$dir" 2>&1; then
        echo "✅ $example_name: SUCCESS"
        ((success_count++))
    else
        echo "❌ $example_name: FAILED"
        ((fail_count++))
        failed_examples+=("$example_name")
    fi
    echo
done

# Summary
echo "=== Summary ==="
echo "Total examples: $total_examples"
echo "Successful: $success_count"
echo "Failed: $fail_count"

if [ $fail_count -eq 0 ]; then
    echo "🎉 All examples ran successfully!"
    exit 0
else
    echo "⚠️  Some examples failed:"
    for example in "${failed_examples[@]}"; do
        echo "  - $example"
    done
    exit 1
fi
