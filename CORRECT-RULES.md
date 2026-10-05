# Correct rules

| Rule | Enforcer |
|---|---|
| A check that scans nothing must fail, never pass | `check_plugin_boundaries.sh` (exits 2 if plugin dir missing or 0 headers scanned; prints scanned count) wired via `tests/Makefile: check-boundaries`, a prerequisite of `test` |
| Plugins use only public APIs (no entity_helper.h / private EntityHelper members / internal headers) | `check_plugin_boundaries.sh` (error names the fix: use ecs.h / PLUGIN_API.md) |
| UI core must not include animation/ui_motion | `tests/ui_boundary_check.sh`, run by `check-boundaries` |
