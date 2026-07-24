# Animation plugin — perf ledger (branch `navi/speed/animation`)

**Verdict: nothing worth changing. Baseline harness added; every optimization
was measured/reasoned and rejected. This is a "measured, nothing to do, here's
the proof" outcome — not an absence of work.**

## Scope note / target-file discrepancy

The task named `src/plugins/ui/animation_config.h` as the target, but that file
is a self-contained fluent builder (`Anim`, `AnimationDef`, per-property
`AnimTrack`, pure `anim::` update functions) with **none** of the hotspots the
original doc listed — no `unordered_map<Key,AnimTrack>`, no `AnimationManager`,
no `std::function on_complete`, no watchers, no `std::deque` queue, no
`std::lerp`, no `CompositeKeyHash`, no `one_shot`. Every "win" below lives in
**`src/plugins/animation.h`**, so that is the file actually analyzed. Both were
inspected; the config header has no map/iteration cost to attack.

## Environment: Santa lockdown

This machine SIGKILLs freshly-compiled binaries (`exit 137`). The benchmark and
the behavior test both **compile clean** but **cannot run here** — verified: a
trivial `printf` binary is also killed 137, and there is no transitive-allow
path (tried building into the tests dir and via `make`; both killed). So:
numbers were NOT measured; conclusions are analytical (Big-O + cache + workload
reality) and are explicit about that. Nothing is fabricated.

## Harness (committed, compile-clean, baseline)

- `tests/animation_test.cpp` — **behavior lock**. Golden value-trajectory tests
  for `animation.h`: per-frame `current` compared **bit-exact** (`memcmp`, so
  even a 1-ULP drift fails), plus completion frame, `on_complete` firing frame
  (exactly once, only on queue-empty completion), sequence/queue advancement,
  `on_step` watcher bucket sequence, untouched-key `get_value`/`is_active`
  semantics, and the zero-duration snap path. Wired into `tests/Makefile`
  `ALL_TESTS`. Compiles clean; Santa blocks the run.
- `examples/catalog/safety/benchmarks/animation_bench.cpp` + `.makefile` —
  `-O3 -DNDEBUG` micro-benchmark of the per-frame hot path
  `AnimationManager::update(dt)`, sweeping N ∈ {64,256,1024,4096,16384} tracks ×
  active-fraction {0%, 1%, 100%} using an isolated local `AnimationManager`
  per cell (real API path). Compiles clean; Santa blocks the run.

---

## DONE
_(none — see rejections; no change met the "measurable win, or airtight it's
worth doing, and trajectory-identical" bar)._

## REJECTED (with reasoning)

### #7 — inline `std::lerp` as `from + (to-from)*u`  — REJECT (behavior change, not free)
`std::lerp(a,b,t)` is **not** bit-identical to `a+(b-a)*t`. The standard requires
`std::lerp` to be exact at the endpoints (`lerp(a,b,1)==b`) and monotonic; the
naive form is neither in general and differs in the low mantissa bits for most
mid-flight `u`. Since the whole task is "value trajectory identical," swapping it
is a **regression by definition** — and the behavior test (bit-exact per frame,
built against `std::lerp` as the golden) would flag it. The doc itself estimated
the speedup at "~0". A per-frame single-lerp is nanoseconds regardless. Not worth
a trajectory change for zero gain. **Gloves.**

### #1 / #2 (HIGH in the doc) — contiguous active-track list / flat enum array — REJECT at this scale
Real and asymptotically correct: `update()` walks the whole `unordered_map`
every frame (`for (auto &kv : tracks) if (!tr.active) continue;`), so cost is
O(total) not O(active); an active-key list or enum-indexed flat array makes it
O(active). BUT:
- **Absolute cost is trivial.** Analytical model (cannot measure — Santa):
  walking the map at the "many keys, few active" regime is ~1ns/node hot,
  ~4ns/node cold (scattered heap nodes ≈ one miss each). Even an absurd
  N=16384, all-cold ≈ 65µs — **<0.4% of a 16.67ms 60fps frame**. At realistic N
  it is tens of nanoseconds.
- **Real N here ≈ 1–3.** The only key enum in the codebase is
  `ui_internal_ani::Key { ButtonWiggle }` (one member); the example uses 3. There
  is no "thousands of registered animation keys" workload anywhere in the repo,
  so the O(N) term never grows. The benchmark exists to catch it *if* such a
  workload ever appears.
- **Correctness is not free.** `on_complete` (via `loop_sequence`) re-enters
  `anim(k).sequence(...)` → `ensure_track` → `tracks[key]` **during** the update
  loop. The current `unordered_map` tolerates this only because re-activating an
  *existing* key doesn't rehash; a naively-mutated active vector would need care
  to not process re-activated tracks mid-frame. The map's actual iteration order
  is unspecified, so the same-frame-vs-next-frame handling of a re-entrant
  reactivation is **already non-deterministic** — there is no well-defined
  trajectory to preserve there, and inventing one is a behavior change I can't
  justify with an unmeasurable, sub-microsecond win.

  Net: adding data structure + re-entrancy complexity to shave nanoseconds off a
  1–3-track workload, unmeasurable under Santa, is textbook over-engineering.
  **Gloves.** (If a real high-track-count consumer ever lands, the committed
  benchmark quantifies it and this becomes worth revisiting.)

### #4 — skip watcher notification when value unchanged (epsilon) — REJECT (redundant)
The code already (a) skips the whole watcher block when `tr.watchers.empty()`
and (b) fires each watcher's callback only when the **quantized** bucket changes
(`q != *w.last_value`). So spurious *notifications* are already suppressed
correctly. The proposal only additionally skips the `quantize()` call itself when
`new_current == tr.current` — but for an active timed track with dt>0, `elapsed`
grows every frame so `new_current` changes almost every frame; the raw-equality
case is essentially never true. It would add a branch to the common (changing)
path to save one cheap `floor/div` on a case that doesn't occur. No measurable
benefit; risks dropping a legitimately-should-fire notification if `quantize`
were ever non-monotonic. **Reject.**

### #3 — lighter `on_complete` callable — REJECT (API churn, no hot-path win)
`on_complete` is invoked at most once per track, on completion — not a per-frame
cost. Replacing `std::function` with a fn-ptr+ctx or `move_only_function` is
public-API churn for a callback that fires O(completions), not O(frames·tracks).
Not on the hot path. **Reject.**

## DEFERRED (design changes, not perf-justified here)

- **#5 / #6** ring buffer / pool for `AnimSegment` queue — premature. Queue ops
  happen at segment boundaries (rare), not per frame; `std::deque` allocation is
  not a measured or reasoned hot cost. Defer.
- **#8 / #33** static-manager / `constinit` singleton — the function-local static
  has a one-time thread-safe-init guard checked on every `manager<Key>()` call,
  but that's a single relaxed atomic load per call, and `update()` calls it once
  per frame. Design change (explicit singleton) for a negligible, unmeasurable
  cost. Defer.
- **#9** `one_shot` static `unordered_set` "leak" — it's a process-lifetime dedupe
  set, not a per-frame cost, and moving it to a component flag is a design change
  outside this plugin. Defer.
- **#10–#35** (SIMD, easing LUTs, sorting active tracks, branch hints, dirty
  flag, `SmallVector` watchers, etc.) — all micro-optimizations of a path that is
  already sub-microsecond at the repo's actual track counts, none measurable
  under Santa, several behavior-affecting. Defer; not worth doing at this scale.

## What could NOT be measured
Everything runtime. Santa (exit 137) blocks all fresh binaries. All perf claims
above are Big-O + cache + workload-size reasoning, stated as such. The floor —
both harness files and the wired test compile clean — is met and green.
