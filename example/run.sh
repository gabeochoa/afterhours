
#!/bin/bash

echo "=== Afterhours Examples Test Runner ==="
echo "Running all example directories..."
echo

# Find all directories with either makefiles or source files (excluding root directory and hidden directories)
all_dirs=$(find . -maxdepth 1 -type d | grep -vE '^\.$' | grep -vE '^\./\.' | while read dir; do
    if [ -f "$dir/makefile" ] || [ -f "$dir/Makefile" ] || find "$dir" -maxdepth 1 -name "*.cpp" -o -name "*.c" | grep -q .; then
        echo "$dir"
    fi
done | sort)

if [ -z "$all_dirs" ]; then
    echo "No directories with makefiles or source files found"
    exit 1
fi

# Count total examples
total_examples=$(echo "$all_dirs" | wc -l)
echo "Found $total_examples example(s) to run:"
echo "$all_dirs" | sed 's|^\./||' | sed 's/^/  - /'
echo

# Track results
success_count=0
fail_count=0
failed_examples=()

# Run each example
for dir in $all_dirs; do
    example_name=$(basename "$dir")
    echo "=== Running $example_name ==="
    
    # Check if directory has a makefile
    if [ -f "$dir/makefile" ] || [ -f "$dir/Makefile" ]; then
        echo "  Found makefile, running make..."
        if make -C "$dir" 2>&1; then
            echo "✅ $example_name: SUCCESS"
            ((success_count++))
        else
            echo "❌ $example_name: FAILED"
            ((fail_count++))
            failed_examples+=("$example_name")
        fi
    else
        echo "  No makefile found, skipping..."
        echo "⚠️  $example_name: SKIPPED (no makefile)"
        # Don't count skipped directories as failures
    fi
    echo
done

# Summary
echo "=== Summary ==="
echo "Total examples: $total_examples"
echo "Successful: $success_count"
echo "Failed/Skipped: $fail_count"

if [ $fail_count -eq 0 ]; then
    echo "🎉 All examples ran successfully!"
    exit 0
else
    echo "⚠️  Some examples failed or were skipped:"
    for example in "${failed_examples[@]}"; do
        echo "  - $example"
    done
    exit 1
fi

