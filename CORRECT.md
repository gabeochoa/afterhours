# CORRECT — afterhours

## Classes (each ≥2 occurrences)
1. **Vacuous/orphan checks pass while scanning nothing.** `check_plugin_boundaries.sh` run from /tmp printed `find: src/plugins: No such file or directory`, scanned 0 headers, exited 0; neither it nor `tests/ui_boundary_check.sh` was referenced by any Makefile/CI (grep: 0 callers). Same shape in `todo.md`: six text-area "defects" were tests misbuilt without `AFTER_HOURS_ENABLE_E2E_TESTING` (injected input silently bypassed).
2. **Plugins copy the direct core include, bypassing the one public entry.** 8 files (`files,modal,pathfinding,settings,telemetry,timer,toast,translation.h`) had `#include "../core/entity_helper.h"` vs `PLUGIN_API.md: use src/ecs.h`.

## Fixes (level + why not higher)
- Class 2 — architecture: migrated all 8 to `../ecs.h` (ecs.h already includes entity_helper.h; one supported way, dead way deleted). Commit `8fd3722`. Higher (types) n/a: include choice isn't type-expressible in C++ headers.
- Class 1 — types/lint via check hardening: script cds to its dir, exits 2 naming the fix if dir missing/0 headers, prints `Scanning 129 plugin headers`; wired as `tests/Makefile check-boundaries`, prerequisite of `test`. Commit `96a3ac2`. Architecture alone can't help: a check in the wrong cwd must refuse, not guess.

## Proof
- Pre-fix: old script from /tmp = exit 0 on 0 files. Post-fix from /tmp = scans 129, passes; probe header with old include = exit 1 naming it; `make check-boundaries` green; `settings_save_test` rebuilt+ran exit 0 (covers settings.h migration).

Rule table: `CORRECT-RULES.md` (created; repo had no CLAUDE.md/AGENTS.md).
