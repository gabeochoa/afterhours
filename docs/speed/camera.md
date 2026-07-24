# Camera Plugin — perf ledger (branch `navi/speed/camera`)

Target: `src/plugins/camera.h` (98 lines). Reviewed with ponytail (full).

**Outcome: nothing worth changing.** This plugin is tiny and already correct.
All three MED items were measured/reasoned and rejected. A harness is committed
for the record. This is a valid "measured, nothing to do, here's proof" result —
no code was changed, so the render output is unchanged by construction.

## Environment note
- `~/homebrew` is the Homebrew prefix on this box (Gabe's MacBook, macOS arm64);
  raylib 5.5 lives at `~/homebrew/Cellar/raylib/5.5`, fmt headers at
  `~/homebrew/include`.
- **Santa lockdown:** freshly-compiled binaries are SIGKILLed (exit 137). Both
  the behavior test and the benchmark **compile and link clean** but **cannot
  run** here. Numbers below are analytical (Big-O / cache / allocation), not
  measured. No numbers were fabricated.

## Harness (committed, compile-clean)
- **Behavior test** — `tests/camera_test.cpp` (raylib-backed; camera's real
  logic is behind `AFTER_HOURS_USE_RAYLIB`). Locks:
  1. `HasCamera` default state (zoom 0.75, zero offset/target/rotation).
  2. Setters write the exact `Camera2D` fields `BeginMode2D` consumes.
  3. **Render-neutral lock:** the transform matrix via
     `raylib::GetCameraMatrix2D` — the exact matrix raylib loads into the GL
     modelview inside `BeginMode2D`. `GetCameraMatrix2D` is a pure CPU function
     (no window/GL), so it runs headless. Any change that alters this matrix for
     a given camera state is a regression.
  4. Singleton registration + both begin/end systems resolving the **same**
     `HasCamera` (begin/end pairing operates on one camera).
  Wired into `tests/Makefile` `ALL_TESTS`/`RAYLIB_TESTS` as `camera_test`.
  Compiles clean (`-Wall -Wextra` + raylib, zero warnings). Run blocked by Santa
  (exit 137).
- **Benchmark** — `examples/catalog/safety/benchmarks/camera_bench.cpp`
  (`-O3 -DNDEBUG`, zero warnings incl. `-Wconversion -Wshadow -Wpedantic`).
  Isolates the only measurable claim (item #1): current code =
  `2× get_singleton_cmp<HasCamera>()` per frame vs. proposed cached-pointer =
  `2× deref` per frame, over 100k frames, with a 5000-entity population so the
  singleton map isn't the only thing resident. Run blocked by Santa (exit 137).
- **Makefile fix (shipped in the harness commit):** `tests/Makefile` hardcoded
  `/opt/homebrew/Cellar/raylib/5.5`, which does not exist on this box — **every**
  raylib test failed to build here. Changed to auto-detect via `brew --prefix
  raylib` / `brew --prefix` (overridable with `make RAYLIB_PREFIX=...`) and added
  `-isystem $(BREW_PREFIX)/include` for header-only fmt. This is the only code
  change on the branch and it is build-only (no runtime/plugin behavior).

---

## DONE
Nothing. No optimization measurably helps or is analytically airtight enough to
justify the risk. See REJECTED.

## REJECTED

### #1 — Cache the `HasCamera` singleton pointer instead of querying every frame
**Rejected.** Negligible cost + a real lifetime hazard.

- **Cost today:** `BeginCameraMode::once()` and `EndCameraMode::once()` each call
  `get_singleton_cmp<HasCamera>()` once per frame → 2 lookups/frame. Each lookup =
  `singletonMap.contains(id)` + `singletonMap.at(id)` on an
  `unordered_map<ComponentID, Entity*>` holding ~1–2 entries (so ~2 hash+probe
  ops, entirely L1-resident) + one `Entity::get<Component>()` array index. That's
  roughly single-digit-to-low-tens of **nanoseconds per frame**. Against a 16.6ms
  (60fps) frame budget this is ~1e-6 of the budget — unmeasurable. `camera_bench.cpp`
  exists to put a real number on it if the box ever allows a run.
- **Risk added by caching:** the singleton entity can be destroyed/recreated on
  scene or screen resets (`delete_all_entities…`, `EnforceSingleton`). A cached
  raw `HasCamera*` would then dangle. The current always-lookup is
  correct-by-construction; it *is* the simplest thing that works (gloves).
- **Verdict:** optimize nothing, keep the lookup.

### #2 — Skip `BeginMode2D`/`EndMode2D` when the camera hasn't changed (dirty check)
**Rejected — this is a correctness misconception, not a speedup.**

- `BeginMode2D` does `rlPushMatrix()` + loads the camera transform; `EndMode2D`
  does `rlPopMatrix()` (confirmed in `~/homebrew/.../rlgl.h`). The matrix stack is
  re-established every frame — these calls must **bracket every frame's**
  world-space draws regardless of whether the camera changed *since last frame*.
- Skipping them when the camera is "unchanged frame-over-frame" would leave world
  draw calls in screen space → broken render. That's a behavior change (a
  regression), and behavior-changing "speedups" are regressions unless intended +
  documented + re-baselined. It is not intended.
- The premise also isn't a cost win: `BeginMode2D` is a couple of matrix ops per
  frame (nanoseconds), dwarfed by the actual draw calls it wraps.
- **Verdict:** do not add a dirty check; it can't be both correct and a skip.

### #3 — Combine `BeginCameraMode` + `EndCameraMode` into one RAII system
**Rejected as a perf item (correctly labeled ergonomics-only in the plan), and
declined on API grounds too.**

- Not a perf win: two `once()` systems in the render pipeline cost two virtual
  dispatches per frame — nanoseconds, unmeasurable.
- The two entry points (`register_begin_camera` / `register_end_camera`) are
  **deliberately separate** so callers register other render systems *between*
  them (the camera brackets the world-space draw phase). A single combined RAII
  system removes that bracket capability — an API regression for a non-win.
- **Verdict:** keep them separate.

## DEFERRED
None. Items #4–#35 in the original brainstorm are LOW and either share the same
"unmeasurable against frame budget" fate as #1 (e.g. #7 null-check removal, #22
cache entity ref, #28 devirtualize, #29 one-vs-two-systems, #34 inline accessors),
depend on features this plugin does not have (#10/#11 lerp/dirty-position, #17
shake, #18 camera stack, #33 history ring buffer), belong to other systems
(#6/#12/#14/#26 world/screen transforms + culling live in the consumers, not
here), or are backend/hardware micro-opts with no evidence of a bottleneck
(#15 SIMD, #25 Metal/Vulkan, #30 alignas). #8 `constexpr` defaults and #31
"remove the stub path" are cosmetic; the stub path is load-bearing for non-raylib
builds (do not remove). None cleared the "measurably helps or is analytically
airtight" bar; none are worth a change to a 98-line file that is already correct.
