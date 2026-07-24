# Timer Plugin — perf ledger

Benchmark-driven review of `src/plugins/timer.h` on branch `navi/speed/timer`.

## TL;DR

The timer plugin is already trivial. `HasTimer::update()` is `current_time += dt;
if (current_time >= reset_time) {...}` and `TriggerOnDt::test()` is the same shape.
The per-frame cost of the timer *systems* is dominated by the ECS's generic
per-entity work in `SystemManager::tick` — for every system, iterate **all**
entities, do a virtual `should_run`, a `std::bitset` component-match
(`HasAllComponents::value`), then a virtual `for_each_with` — not by the timer
arithmetic. Micro-optimizing the arithmetic (branchless max, reciprocal, SIMD,
`[[likely]]`) therefore cannot move the real number: it's a rounding error next
to the dispatch that already had to happen to reach it.

**Could not collect runtime numbers on the dev machine:** Santa runs in
**Lockdown** mode and SIGKILLs every freshly-compiled binary (exit 137) — verified
against a trivial `int main(){return 0;}`. The benchmark
(`examples/catalog/timer/benchmarks/`) and behavior test (`tests/timer_test.cpp`)
both **compile clean** and are ready to run on an unlocked machine. Items below are
triaged analytically (instruction count / dispatch model / iteration count). No
numbers were fabricated.

Legend: **DONE** (measured/reasoned win, implemented) · **REJECTED** (no win / YAGNI,
with reason) · **DEFERRED** (needs runtime data this box can't produce).

---

1. **Skip `TimerUpdateSystem` iteration for expired non-auto-reset timers.**
   **REJECTED.** The iteration over all entities happens in `SystemManager::tick`
   *before* any timer-specific code; a system cannot cheaply skip an entity — the
   virtual dispatch + bitset match already ran to get into `for_each_with`. Skipping
   the `current_time += dt` for an already-expired timer saves ~1 add + 1 compare,
   swamped by the dispatch that reached it. And to know it's expired you must read
   the component anyway. No measurable win.

2. **Dirty flag to skip update when paused.** **REJECTED.** `update()` already
   early-returns on `paused` with a single branch. A separate dirty flag adds state
   to maintain (set/clear on pause/resume) for zero arithmetic saved — the branch is
   already there and perfectly predicted for a mostly-unpaused population. Gloves.

3. **Batch all timer updates into a single SIMD pass.** **REJECTED (cargo-cult).**
   Components live behind per-entity `Entity` objects reached through the ECS
   iteration, not in a contiguous `float[]`. There is no array to vectorize without
   first building one (an SoA rewrite, see #30). Even then, the work is one FMA +
   compare per element — memory-bound, and the ECS dispatch dominates regardless.
   SIMD here optimizes the 1% that isn't the bottleneck.

4. **Sorted priority queue for expiration instead of checking all timers.**
   **REJECTED (cargo-cult for this design).** A heap replaces an O(N) linear scan of
   trivial adds with O(log N) per-tick reinsertion, pointer chasing, and heap
   maintenance — for auto-reset timers you'd re-push every element every period.
   For N up to 100k of `+= dt`, the linear scan wins on cache behavior and constant
   factors. This is the classic "timer wheel for a metronome" mistake.

5. **Combine `TimerUpdateSystem` and `TriggerUpdateSystem` into one pass.**
   **REJECTED (changes semantics; no free win).** The two systems match *different*
   component sets (`HasTimer` vs `TriggerOnDt`), which in practice are largely
   *different entities*. A single `System<HasTimer, TriggerOnDt>` only matches
   entities that have **both** — not equivalent behavior. You cannot fuse the two
   full-entity iterations into one without either (a) changing which entities get
   updated, or (b) building a custom system that manually checks both components per
   entity (which still iterates all entities once and re-does both bitset tests).
   The only real saving would be one of the two full iteration passes, but only for
   the subset of entities carrying both components — a niche that doesn't match how
   the plugin is used. Not worth the coupling. DEFERRED if a real workload is shown
   where the same entities carry both components at high N.

6. **CRTP to avoid virtual dispatch in `for_each_with`.** **REJECTED here.** This is
   an ECS-core change, not a timer-plugin change — it would touch `System<>` for
   every plugin, well outside this scope. It *is* the one thing that could matter
   (the virtual `for_each_with` per entity is real overhead), but it's a
   framework-wide refactor with broad blast radius. DEFERRED to a core-ECS effort,
   not a timer PR.

7. **Epsilon float compare instead of `>=`.** **REJECTED.** Changes correctness
   semantics (introduces slop into "is the timer done"), costs an extra subtract +
   compare, and fixes a problem nobody has. YAGNI.

8. **Pre-compute `1.0f / reset_time` for progress.** **REJECTED.** `get_progress()`
   is not on the hot update path — it's a query called by UI/gameplay when needed,
   not per-entity per-frame by the systems. Caching a reciprocal adds a field and a
   maintenance burden (recompute on `reset_time` change) to speed up a cold call.

9. **Branchless max in `get_remaining()`.** **REJECTED.** Same as #8: not on the hot
   path. `std::max(0.0f, x)` already lowers to a single `maxss` on x86 / `fmax` —
   there's no branch to remove.

10. **`[[likely]]` on the `!paused` branch.** **REJECTED (noise).** The branch is
    already trivially predicted for a steady-state population; a hint changes code
    layout at best and cannot beat the ECS dispatch cost. Measure-expect-zero item.

11. **Inline `HasTimer::update()`.** **REJECTED (already effectively inlined).** It's
    a header-defined member; at `-O3` it inlines into `for_each_with` already. No
    action needed. (Marking it `inline` explicitly is a no-op for member functions.)

12. **`void` overload of `update()` when return unused.** **REJECTED.** The `bool`
    return is computed from a compare that must happen anyway; returning it is free
    (in a register). A second overload is API surface for zero gain.

13. **Pack `HasTimer` fields.** **REJECTED (already tight).** Layout is
    `float,float,bool,bool` = 10 bytes → 12 with padding. Reordering can't beat that;
    the two bools can't pack below a byte each without bitfields, which cost
    read-modify-write on access. `HasTimer` also inherits `BaseComponent`, so its
    footprint is dominated by the base, not these fields.

14. **`alignas(16)` on timer component arrays for SIMD.** **REJECTED (cargo-cult).**
    There are no timer component *arrays* (see #3/#30); components are per-entity.
    Aligning nothing for SIMD that isn't happening.

15. **Avoid iterating entities with only expired non-auto-reset timers.** **REJECTED.**
    Same as #1 — the iteration is generic and in the ECS core; the plugin can't opt
    entities out cheaply.

16. **Event-based expiration instead of per-frame polling.** **REJECTED (YAGNI).**
    An event/callback system is far more machinery than `+= dt`, adds allocation and
    indirection, and only pays off when N is huge AND periods are long AND most
    timers are idle — not the general case. Gloves.

17. **Profile branch misprediction on `is_ready()`.** **DEFERRED (can't run here).**
    This is a measurement task; Santa Lockdown blocks running the profiler-able
    binary. Analytically the compare is cheap and the branch outcome is stable within
    a population, so misprediction is unlikely to dominate. No code change proposed.

18. **`constexpr` default field values.** **REJECTED (no-op).** In-class initializers
    for `float`/`bool` are already compile-time constants; `constexpr` adds nothing
    to a runtime-constructed component.

19. **Group timers by expiration for batch processing.** **REJECTED.** Same family as
    #4/#19/#22 — bucketing/scheduling machinery to avoid a linear scan that is
    already cache-optimal for trivial work. YAGNI.

20. **Iterate the component array directly (avoid per-entity virtual dispatch).**
    **REJECTED here / DEFERRED to core.** Same as #6 — this is the ECS iteration
    model, not the timer plugin. Real but out of scope; framework-wide change.

21. **`[[gnu::pure]]` on `is_ready()`/`get_progress()`.** **REJECTED (noise).** These
    are tiny header functions already inlined at `-O3`; `pure` gives the optimizer
    nothing it doesn't already see. Non-portable attribute for zero win.

22. **Timer wheel for many timers of different durations.** **REJECTED (cargo-cult).**
    See #4. A timer wheel shines for sparse, long-lived, event-driven timers (kernel
    timeouts). For a dense `+= dt` sweep it's pure overhead and complexity.

23. **Pre-allocate timer storage by expected entity count.** **REJECTED (scope).**
    Storage is the ECS entity pool's concern, not the timer plugin's; the plugin
    doesn't own an array to reserve.

24. **Cache `path_size` instead of `path.empty()` on a deque.** **REJECTED (N/A).**
    There is no `path`/deque anywhere in `timer.h`. This item appears mis-filed from
    another plugin.

25. **`static_assert` HasTimer trivially copyable.** **REJECTED (false + no perf).**
    `HasTimer` derives from `BaseComponent` (has virtuals / non-trivial base), so it
    is *not* trivially copyable; the assert would fail. Adds no runtime speed anyway.

26. **Tune fixed tick rate for precision vs performance.** **REJECTED (scope).** The
    tick rate is a `SystemManager` global affecting all systems, not a timer-plugin
    knob. Out of scope and not a timer-specific win.

27. **Skip `TriggerOnDt::test()` when `current` is far from `reset`.** **REJECTED.**
    To know it's "far" you must read `current` and compare to `reset` — which is
    exactly the work `test()` already does. You'd add a branch to skip a branch. No
    win.

28. **Monotonic clock instead of accumulating `dt`.** **REJECTED (wrong semantics).**
    These timers are simulation timers driven by the game's `dt` (pausable,
    time-scalable, deterministic for replay). A wall-clock monotonic source would
    break pause, time-scaling, and determinism. Not a perf question.

29. **Global timer array instead of per-entity components.** **REJECTED (cargo-cult).**
    Abandons the ECS model for a parallel data structure that must be kept in sync
    with entity lifetime. Large complexity to turn a per-entity component into a
    global — for arithmetic that isn't the bottleneck. Gloves.

30. **SoA layout for batch updates.** **REJECTED (cargo-cult for this scope).** An SoA
    rewrite is an ECS-storage change, not a timer change, and only pays off if the
    update body were the bottleneck (it isn't — dispatch is). Would enable #3's SIMD
    to matter marginally, at the cost of a storage redesign. Not worth it.

31. **Multiply by pre-computed reciprocal in `get_progress()`.** **REJECTED.** See #8 —
    cold query path, and it adds a cached field to maintain. A single `divss` on a
    cold call is not worth denormalizing state.

32. **Cache `dt` in a register for the pass.** **REJECTED (already happens).** `dt` is
    a `float` value parameter; the compiler keeps it in a register across the loop at
    `-O3`. Nothing to do.

33. **`__builtin_expect` for rare `reset_time <= 0` case in `get_progress()`.**
    **REJECTED (noise + cold path).** `get_progress()` isn't on the hot system path,
    and the guard is already cheap. Non-portable hint for zero measurable win.

34. **`double` accumulators to avoid precision loss.** **REJECTED (not a perf item,
    and a pessimization).** Doubling the accumulator size hurts cache/bandwidth (the
    thing that actually matters here) to fix precision drift that only appears over
    very long single-timer lifetimes — and auto-reset timers reset `current_time`
    every period, bounding drift. Wrong tradeoff for a perf pass.

35. **Coalesce adjacent timers in memory for the prefetcher.** **REJECTED.** Same
    family as #30/#35 — a storage-layout change outside the plugin, chasing a
    memory-locality win on a workload dominated by dispatch, not bandwidth.

---

## What would actually matter (and why it's out of scope)

The only genuinely real overhead in the timer *systems* is the **generic ECS
per-entity dispatch**: iterating all entities per system + virtual
`should_run`/`for_each_with` + the `std::bitset` component match. Items #6/#20
point at this correctly, but fixing it is a **core `System<>`/ECS change** with
fleet-wide blast radius across every plugin — not a timer PR. If a real profile on
an unlocked machine shows the timer systems in the hot path, the right move is a
core-ECS iteration/dispatch improvement, benchmarked against all plugins, not a
timer-local hack.

## Verdict

No timer-plugin-local optimization from this list is worth implementing. The
valuable output of this pass is the **benchmark + behavior test** (committed), which
(a) prove the code is correct today and (b) are ready to produce real numbers on an
unlocked machine to confirm this analysis. See `docs/speed/timer_results.md` for
methodology and what remains to be measured.
