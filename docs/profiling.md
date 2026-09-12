# Profiling

Build every translation unit with `AFTERHOURS_ENABLE_PROFILING=1` to enable
system timing subscriptions and the default profiler. The default is 0. Use the
same value throughout a program, since it changes inline instrumentation.

Include `src/plugins/profiling.h`, create a `profiling::Collector`, call
`start()`, and call `end_frame()` once after the entire application frame.
Multiple SystemManagers can contribute to that frame. Alternatively pass an
explicit wall-clock duration in milliseconds to `end_frame(ms)`. Simulation
delta time is not a frame-performance measurement.

Collectors and their SystemManagers run on the same thread. Subscriptions are
thread-local, support up to eight simultaneous collectors, and do not replace
`SystemManager::set_profile_hook`. Check `start()` for subscription-capacity
failure. `stop()` retains the last data; `reset()` clears it and keeps the prior
recording state. Stopping or resetting from a system callback is supported.
Collector instances cannot be moved or copied.

`Options` bounds frame history (600 by default), distinct system/phase entries
(256), and custom counters (32). Once warmed up, system recording performs no
allocations. Overflow is reported in `Snapshot::dropped_system_samples`.
Snapshots own their values and sort systems by recent average time per frame.
`SystemSample::recent_average_frame_ms` covers the retained history;
`average_frame_ms` covers all completed recording frames since reset. Both
combine repeated calls and include zero for frames where a system did not run.
The window length is `Snapshot::frame_ms.size()`. The raw totals and mean per
call remain available for custom consumers. Snapshots exclude unfinished-frame
work, and stopping discards that pending work. Timings
are inclusive, so nested manager work can overlap; do not sum inclusive rows
as if they were exclusive frame cost. Recording starts with the next complete
system scope. Frame percentiles and FPS use the bounded rolling history.

macOS CPU and resident memory are sampled at most twice a second. CPU is process
CPU time divided by elapsed wall time, with 100% representing one core. It can
exceed 100%. Unsupported platforms and the initial CPU interval return empty
optionals. Set `Options::sample_process=false` to disable these probes.

Use `AFTERHOURS_PROFILE_COUNTER(collector, "Queue", "jobs", expression)` for
custom counters. Its expression is not evaluated while stopped or compiled out.
Direct method arguments follow ordinary C++ evaluation rules.

Include `src/plugins/ui/profiler.h` for `ui::imm::profiler_panel(context, parent,
collector, state, options, config)`. Keep `ProfilerState` between frames. The
panel provides start/stop, pause/resume view, reset, recent/overall average
sorting, frame history, percentiles, system timings, process metrics and custom
counters. The table shows recent and overall average milliseconds per frame
and the latest completed frame. The header shows the recent window length.
Pausing the view does not stop recording; hiding the panel does not stop it
either. Snapshots refresh every rendered frame by default. Set
`ProfilerOptions::refresh_hz` to a positive rate such as 120 to cap refreshes,
or 0 to refresh every frame. The render loop limits the actual rate. This
does not change collection frequency or the twice-per-second process probes.
`ProfilerOptions::font_size` controls text size and chart-label sizing. The
default is 18px with up to six system rows. `ProfilerOptions` also controls
chart, system and counter sections and the row limit. UI colors come
from the ordinary theme; layout comes from `ComponentConfig`.

`profiler_view` accepts a caller-owned `Snapshot` without installing a collector,
so another profiler can use the same display. The current table abbreviates
long system names; full names remain in the snapshot and E2E dump.

With the macro set to 0, the collector has no recording storage, new timing
scopes and observers are absent, counter expressions are removed, and the
panel emits no UI. Existing application profiling hooks remain independent.
`wm` enables profiling support by default. Its `make run` target starts
recording and opens the live panel; F3 toggles visibility. The
`system_profile_lab` demonstrates background recording and a CPU workload.
Build wm with `ENABLE_PROFILING=0` to verify the disabled configuration.

`e2e_testing/perf_commands.h` adapts the default collector. Its built-in provider
reports mean milliseconds and calls separately from entity counts. Apps with
custom providers can continue supplying them directly.

The benchmark in `tests/profiling_benchmark.cpp` exercises 32 systems with
compiled-out, stopped, recording and panel modes. Run
`nice -n 10 make -C tests -j2 profiling-benchmark` for UI construction/layout
measurements. `profiling-render-benchmark` uses raylib and waits for GPU
completion each frame; pass `PROFILING_FONT` with a font file path. The second
binary argument sets the measured frame count for longer runs. See
[profiling-measurements.md](profiling-measurements.md) for results and limits.

The benchmark accepts an optional fifth argument for the refresh cap. For
example, `profiling_benchmark_on panel 3000 "" 120` measures UI construction
with a 120 Hz cap; use 0 for every frame or 4 for the previous update cadence.
The renderer benchmark uses the font path as its fourth argument. The current
benchmark uses the wm overlay's 20px text and five displayed system rows.
