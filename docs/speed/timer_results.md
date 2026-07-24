# Timer Plugin — benchmark methodology & results

## Status: harness ready, runtime numbers NOT collected on the dev machine

**Why no numbers:** the development machine (`cli:boulder`, macOS arm64) runs
**Santa in Lockdown mode**, which SIGKILLs any freshly-compiled, un-allowlisted
binary. Every attempt to run a just-built binary — including a trivial
`int main(){return 0;}` — exits with **137 (SIGKILL)**. Both the behavior test and
the benchmark **compile clean** but cannot execute here. No numbers were fabricated.

To get real data, build and run on an unlocked machine:

```
# behavior test
cd tests && make timer_test && ./timer_test

# benchmark (-O3 -DNDEBUG)
cd examples/catalog/timer/benchmarks && make
# or: clang++ -std=c++23 -O3 -DNDEBUG main.cpp -o timer_benchmarks.exe && ./timer_benchmarks.exe
```

## Benchmark design (`examples/catalog/timer/benchmarks/main.cpp`)

Catch2 microbenchmarks, `-O3 -DNDEBUG`, sweeping **N = 1k / 10k / 100k** timer
entities:

| Benchmark | Path exercised |
|---|---|
| `TimerUpdateSystem tick` | Real per-frame path via `SystemManager::run` (all-entity iteration + virtual dispatch + `HasTimer::update`) |
| `TriggerUpdateSystem tick` | Same, for `TriggerOnDt::test` |
| `Timer+Trigger both systems tick` | Two systems registered — the target scenario for item #5 |
| `raw HasTimer::update loop` | `update()` over a contiguous `std::vector`, **no ECS/virtual dispatch** — floor cost of the arithmetic |
| `raw TriggerOnDt::test loop` | Same for `test()` |

Timers use `auto_reset=true` (period 0.5s, dt 0.16s) so the systems do real
fire+re-arm work rather than short-circuiting.

## The measurement that decides everything

Compare **`raw *_loop`** vs **`*UpdateSystem tick`** at the same N:

- If `system tick` ≫ `raw loop`, the bottleneck is **ECS iteration + dispatch**, and
  any micro-opt to the timer arithmetic (branchless max, reciprocal, `[[likely]]`,
  SIMD on the body) is provably incapable of moving the real number. This is the
  expected outcome given the code (see below), and the basis for rejecting the
  arithmetic-level items in `timer.md`.
- If they're close, the arithmetic is a real fraction and a couple of micro-opts
  *might* register — re-run per-opt and keep only measured wins.

## Analytical prediction (why the rejections stand without numbers)

`SystemManager::tick` (src/core/system.h) runs, **per system, per frame**:

```
for each entity in ALL entities:
    if (!entity) continue;                       # null check
    system->for_each(entity, dt):                # (virtual-ish) 
        tags_ok(entity)                          # tag bitset checks
        HasAllComponents<...>::value(entity)     # std::bitset component match
        -> for_each_with(...)                    # VIRTUAL dispatch
             timer.update(dt)                    # current += dt; compare  <-- the "work"
```

The actual timer work is ~1 add + 1 compare (+ maybe a reset store). Everything
above it — the unconditional all-entity walk, the bitset match, and the virtual
`for_each_with` — runs regardless and dwarfs it. So:

- **Micro-opts to the body (#7–#12, #21, #27, #31–#34):** optimize the ~1% that isn't
  the bottleneck → expected ~0.
- **SIMD / SoA / alignment (#3, #14, #30, #35):** no contiguous array exists to
  vectorize; components are per-entity behind the ECS. No-ops without a storage
  rewrite, which is out of scope and still wouldn't beat dispatch.
- **Priority queue / timer wheel / grouping (#4, #19, #22):** replace a cache-friendly
  linear scan of trivial adds with log-N pointer-chasing bookkeeping — a pessimization
  for dense periodic timers.
- **Skip-expired / paused-dirty (#1, #2, #15):** the plugin can't cheaply skip the
  generic per-entity iteration, and `update()` already early-returns on pause.
- **Combine systems (#5):** the two systems match different component sets (different
  entities); fusing them changes semantics, not just cost.
- **CRTP / direct array iteration (#6, #20):** the *one* real overhead (virtual
  dispatch + generic iteration), but it lives in the ECS core `System<>`, not the
  timer plugin — a fleet-wide change, deferred to a core effort.

## Behavior test (`tests/timer_test.cpp`)

Captures current correctness so any future refactor is regression-checked:
fires at `reset_time`; non-auto-reset stays ready; auto_reset re-arms; paused
doesn't advance (`update` + `add_time`); `get_progress` (incl. `reset_time<=0`
→ 1.0 guard); `get_remaining` clamps at 0; `set_time`/`reset`; `TriggerOnDt`
one-shot + auto-reset to 0; and both update systems driven over real entities
via `SystemManager`. Wired into `tests/Makefile` (`ALL_TESTS`). Compiles clean;
run on an unlocked box to confirm green.
