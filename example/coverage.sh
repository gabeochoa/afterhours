#!/bin/bash

# Coverage Tracking Script for Afterhours Library
# Analyzes which headers are exercised by examples

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$SCRIPT_DIR/../src"
EXAMPLE_DIR="$SCRIPT_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "=== Afterhours Library Coverage Report ==="
echo ""
echo "Scanning examples for header inclusion..."
echo ""

# Track totals
total_headers=0
covered_headers=0
uncovered_headers=0

# Function to check if a header is included in any example
check_header_coverage() {
    local header="$1"
    local header_name=$(basename "$header")
    local count=$(grep -r "#include.*$header_name" "$EXAMPLE_DIR" --include="*.cpp" --include="*.h" 2>/dev/null | wc -l | tr -d ' ')
    echo "$count"
}

# Function to print coverage for a directory
print_section() {
    local section_name="$1"
    local section_dir="$2"

    if [ ! -d "$section_dir" ]; then
        return
    fi

    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "📁 $section_name"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

    for header in "$section_dir"/*.h; do
        if [ -f "$header" ]; then
            ((total_headers++))
            local name=$(basename "$header")
            local count=$(check_header_coverage "$header")

            if [ "$count" -gt 0 ]; then
                ((covered_headers++))
                printf "${GREEN}✓${NC} %-35s %3d example(s)\n" "$name" "$count"
            else
                ((uncovered_headers++))
                printf "${RED}✗${NC} %-35s %3d example(s)\n" "$name" "$count"
            fi
        fi
    done
    echo ""
}

# Check core headers
print_section "Core ECS (src/core/)" "$SRC_DIR/core"

# Check utility headers (src/)
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📁 Utility Headers (src/)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
for header in "$SRC_DIR"/*.h; do
    if [ -f "$header" ]; then
        ((total_headers++))
        name=$(basename "$header")
        count=$(check_header_coverage "$header")

        if [ "$count" -gt 0 ]; then
            ((covered_headers++))
            printf "${GREEN}✓${NC} %-35s %3d example(s)\n" "$name" "$count"
        else
            ((uncovered_headers++))
            printf "${RED}✗${NC} %-35s %3d example(s)\n" "$name" "$count"
        fi
    fi
done
echo ""

# Check plugin headers
print_section "Plugins (src/plugins/)" "$SRC_DIR/plugins"

# Check memory headers
print_section "Memory (src/memory/)" "$SRC_DIR/memory"

# Check UI headers
print_section "UI (src/plugins/ui/)" "$SRC_DIR/plugins/ui"

# Print summary
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📊 SUMMARY"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

if [ "$total_headers" -gt 0 ]; then
    coverage_pct=$((covered_headers * 100 / total_headers))
else
    coverage_pct=0
fi

echo "Total headers scanned: $total_headers"
printf "${GREEN}Covered:${NC}   $covered_headers\n"
printf "${RED}Uncovered:${NC} $uncovered_headers\n"
echo ""

# Coverage bar
bar_width=40
filled=$((coverage_pct * bar_width / 100))
empty=$((bar_width - filled))

printf "Coverage: ["
printf "${GREEN}"
for ((i=0; i<filled; i++)); do printf "█"; done
printf "${RED}"
for ((i=0; i<empty; i++)); do printf "░"; done
printf "${NC}] %d%%\n" "$coverage_pct"
echo ""

# List uncovered headers if any
if [ "$uncovered_headers" -gt 0 ]; then
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    printf "${YELLOW}⚠ Uncovered headers need examples:${NC}\n"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

    # Re-scan for uncovered
    for header in "$SRC_DIR/core"/*.h "$SRC_DIR"/*.h "$SRC_DIR/plugins"/*.h "$SRC_DIR/memory"/*.h "$SRC_DIR/plugins/ui"/*.h; do
        if [ -f "$header" ]; then
            count=$(check_header_coverage "$header")
            if [ "$count" -eq 0 ]; then
                echo "  - $(basename "$header")"
            fi
        fi
    done 2>/dev/null
    echo ""
fi

# Example directory count
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📁 EXAMPLE DIRECTORIES"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

count_examples() {
    local dir="$1"
    local name="$2"
    if [ -d "$dir" ]; then
        count=$(find "$dir" -mindepth 1 -maxdepth 1 -type d 2>/dev/null | wc -l | tr -d ' ')
        printf "  %-20s %3d examples\n" "$name" "$count"
    fi
}

count_examples "$EXAMPLE_DIR/core" "core/"
count_examples "$EXAMPLE_DIR/memory" "memory/"
count_examples "$EXAMPLE_DIR/utility" "utility/"
count_examples "$EXAMPLE_DIR/plugins" "plugins/"
count_examples "$EXAMPLE_DIR/ui" "ui/"
count_examples "$EXAMPLE_DIR/safety" "safety/"

total_examples=$(find "$EXAMPLE_DIR" -name "makefile" -type f 2>/dev/null | wc -l | tr -d ' ')
echo ""
echo "Total example directories: $total_examples"
echo ""
echo "=== End of Coverage Report ==="
