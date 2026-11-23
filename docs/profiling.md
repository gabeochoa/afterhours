# Profiling

Build every translation unit with `AFTERHOURS_ENABLE_PROFILING=1`, default 0.
Include `src/plugins/profiling.h`, own a `profiling::Collector`, check `start()` and
call `end_frame()` once after the complete application frame. `end_frame(ms)` accepts
wall-clock milliseconds, not simulation dt. Multiple managers can contribute.

## Scoped zones

Any code can open a nested zone with
`AFTERHOURS_PROFILE_ZONE("name")` (or `profiling::ScopedZone zone{name};`),
optionally with a `SystemPhase`. At destruction the zone's wall-clock
milliseconds are delivered to every subscribed `Collector` exactly like a
system sample, so zones nest: a child zone's duration never exceeds its
parent's. With profiling compiled out the macro and class are inert no-ops,
so zone calls need no `#if` at their call sites. `AFTERHOURS_PROFILE_COUNTER`
already covers spot values; zones are for whole blocks of work.

## Collection contract

- Collector and managers share a thread. Up to eight thread-local subscriptions can
  coexist without replacing `SystemManager::set_profile_hook`. Collectors cannot move/copy.
- `stop()` retains completed data and discards pending samples; `reset()` clears data
  while preserving recording state. Stop/reset from a system callback is supported.
- `Options` bounds history at 600 frames, system/phase entries at 256 and counters at 32
  by default. Overflow appears in `Snapshot::dropped_system_samples`. Warm recording
  reuses storage; system histories allocate on first appearance.
- Recent/overall averages include zero for skipped systems and combine repeated calls
  per frame. Recent uses retained history, overall uses completed frames since reset.
  Snapshots exclude unfinished work. Inclusive nested timings can overlap; do not sum
  them as exclusive frame cost. FPS/percentiles use recent history.
- macOS process CPU/RAM sample at most twice a second. 100% CPU means one core.
  Unsupported/initial samples are empty optionals; `sample_process=false` disables probes.
- `AFTERHOURS_PROFILE_COUNTER(collector, "Queue", "jobs", expression)` avoids evaluating
  the expression when stopped/compiled out. Direct arguments obey normal C++ evaluation.

## Display

Include `src/plugins/ui/profiler.h`; call
`ui::imm::profiler_panel(context, parent, collector, state, options, config)` with
persistent `ProfilerState`. Start/stop controls recording; pause/resume freezes only
the view. Hiding the panel does not stop recording. The chart supports inspecting a
frozen frame interval. Tables show recent/overall average and latest completed frame.

`ProfilerOptions` controls sections, row count, font size and refresh cap. Defaults
are 18px, six rows, and every rendered frame. `refresh_hz=120` caps refreshes, while
0 refreshes each frame. Neither changes collection or process-probe frequency.
`profiler_view` accepts a caller-owned snapshot for another collector. Full system
names remain in snapshots/E2E even when abbreviated on screen.

With profiling compiled out, its storage, instrumentation, observers, counter
expressions and UI are absent. Independent application hooks remain available.
WM records with `make run`, starts hidden, and toggles with F3; `ENABLE_PROFILING=0`
compiles it out.

## E2E and benchmarks

`src/plugins/e2e_testing/perf_commands.h` adapts the collector. Custom providers can
supply optional FPS, p99 and top entries through `set_provider(...)`; explicitly call
`register_perf_commands(sm)`. Commands are `dump_profile [count]`,
`dump_profile --json`, `reset_profile`, `expect_fps_above <fps>` and
`expect_p99_below <ms>`. `--json` logs one single-line JSON object with the
full builtin-collector snapshot (frames, fps, percentiles, per-system
mean/average/recent/peak/last/calls, counters) for machine consumers; a
snapshot-diff tool is deliberately deferred. `reset_profile` clears
frames/systems/counters while recording continues, so the next dump covers
only the window after it. Missing metrics must stay unavailable.
`tools/sample_to_collapsed.py` converts macOS sample output.

`profiling::snapshot_to_collapsed(snapshot)` serializes a snapshot to
folded-stack lines (`frame;<phase>;<system> <total_ms>`, aggregated and sorted
by stack) for flamegraph.pl. Snapshots hold per-system totals only, so stacks
are one level deep; zero totals are omitted.

```sh
nice -n 10 make -C tests -j2 profiling-benchmark
nice -n 10 make -C tests -j2 profiling-render-benchmark PROFILING_FONT=/path/to/font.ttf
```

The layout benchmark excludes drawing. The renderer benchmark uses Raylib and waits
for GPU completion. Optional arguments select frame count, font and refresh cap;
see [measurements](profiling-measurements.md) for the exact runs and limits.
