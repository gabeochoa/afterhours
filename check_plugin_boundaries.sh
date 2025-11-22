#!/bin/bash

# Script to check that plugins only use public APIs
# Scans src/plugins/ for violations

set -e

PLUGIN_DIR="src/plugins"
VIOLATIONS=0

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "Checking plugin API boundaries..."

# Check for direct includes of entity_helper.h
echo "Checking for direct entity_helper.h includes..."
while IFS= read -r file; do
  if grep -q '#include.*entity_helper\.h' "$file"; then
    echo -e "${RED}VIOLATION:${NC} $file includes entity_helper.h directly"
    echo "  Use 'ecs.h' instead"
    grep -n '#include.*entity_helper\.h' "$file" | sed 's/^/    /'
    VIOLATIONS=$((VIOLATIONS + 1))
  fi
done < <(find "$PLUGIN_DIR" -name "*.h" -type f)

# Check for direct access to private members
echo ""
echo "Checking for direct access to private EntityHelper members..."

PRIVATE_MEMBERS=(
  "EntityHelper::singletonMap"
  "EntityHelper::entities_DO_NOT_USE"
  "EntityHelper::permanant_ids"
  "EntityHelper::temp_entities"
  "EntityHelper::get\\(\\)\\.singletonMap"
  "EntityHelper::get\\(\\)\\.entities_DO_NOT_USE"
  "EntityHelper::get\\(\\)\\.permanant_ids"
  "EntityHelper::get\\(\\)\\.temp_entities"
)

for member in "${PRIVATE_MEMBERS[@]}"; do
  while IFS= read -r file; do
    if grep -q "$member" "$file"; then
      echo -e "${RED}VIOLATION:${NC} $file accesses private member: $member"
      grep -n "$member" "$file" | sed 's/^/    /'
      VIOLATIONS=$((VIOLATIONS + 1))
    fi
  done < <(find "$PLUGIN_DIR" -name "*.h" -type f)
done

# Check for includes of internal/private headers
echo ""
echo "Checking for includes of internal/private headers..."
while IFS= read -r file; do
  if grep -q '#include.*\(internal\|private\)' "$file"; then
    echo -e "${RED}VIOLATION:${NC} $file includes internal/private header"
    grep -n '#include.*\(internal\|private\)' "$file" | sed 's/^/    /'
    VIOLATIONS=$((VIOLATIONS + 1))
  fi
done < <(find "$PLUGIN_DIR" -name "*.h" -type f)

# Summary
echo ""
if [ $VIOLATIONS -eq 0 ]; then
  echo -e "${GREEN}✓ All plugins comply with API boundaries${NC}"
  exit 0
else
  echo -e "${RED}✗ Found $VIOLATIONS violation(s)${NC}"
  echo ""
  echo "See PLUGIN_API.md for allowed APIs and patterns."
  exit 1
fi

