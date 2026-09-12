# Profiling measurements

Initial collector measurements, before per-system rolling averages, taken
September 12, 2026 on Apple M4/macOS with clang++, C++20, `-O2`,
and `nice -n 10`. Five runs per mode, alternating order; values below are
medians across runs. Each run warms 200 frames, then measures 3,000 frames
of 32 named systems doing 100 sine operations each.

| Mode | Frame p50 ms | Frame p99 ms | C++ allocations/frame | CPU seconds / 3,000 frames | Resident MB | Resident growth MB |
|---|---:|---:|---:|---:|---:|---:|
| Compiled out | 0.079750 | 0.087625 | 0 | 0.2399 | 1.58 | 0.00 |
| Compiled in, stopped | 0.080000 | 0.087083 | 0 | 0.2407 | 1.64 | 0.00 |
| Background recording | 0.088209 | 0.098500 | 0 | 0.2656 | 1.66 | 0.00 |
| Recording plus panel construction/layout | 0.200458 | 0.237250 | 31.01 | 0.5724 | 2.30 | 0.11 |

Background recording added about 8.5 microseconds to the median frame in this
workload, with no steady-state allocations or measured resident growth. The
stopped difference was about 0.25 microseconds, small enough to treat cautiously.
Panel construction/layout added about 112 microseconds over recording alone.

Panel mode excludes drawing, GPU presentation and font rasterization. It uses
the library UI layout with the test backend. The snapshot refreshes four times
per second, and the 600-frame history finishes filling during measurement.
Allocation counts cover C++ `new`, not every allocation in platform APIs. These
short runs establish the collector cost for this workload; they are not a
frame-time guarantee for other applications.

Reproduce with `nice -n 10 make -C tests -j2 profiling-benchmark` from afterhours.
The target builds both compile configurations and runs all four modes. Run the
resulting binaries repeatedly and alternate order for comparisons. Keep builds
and other agent workloads out of the measurement interval.

## Drawing the panel

Three alternating runs per mode used the same workload and a 1280x720 raylib
headless render target. Every frame draws and calls `glFinish`, including the
empty-panel modes. Panel mode draws the real chart, labels and table. The UI
test harness supplies fixed text measurements; the renderer uses Gaegu Bold.
This measures offscreen rendering through GPU completion, not window-system
presentation. Values are medians across the three runs.

| Mode | Frame p50 ms | Frame p99 ms | C++ allocations/frame | CPU seconds / 3,000 frames | Resident MB | Resident growth MB |
|---|---:|---:|---:|---:|---:|---:|
| Compiled out | 0.813208 | 1.768708 | 8.00 | 0.8824 | 28.83 | 0.02 |
| Compiled in, stopped | 0.750458 | 1.943459 | 8.00 | 0.8201 | 29.08 | 0.05 |
| Background recording | 0.739166 | 1.942834 | 8.00 | 0.8023 | 28.95 | 0.02 |
| Recording plus rendered panel | 1.613834 | 5.086834 | 81.22 | 3.3127 | 29.59 | -2.22 |

Rendering noise exceeds the collector difference in this run. The faster
recording result is not evidence of a speedup. Drawing the panel adds about
0.87 ms to the median frame over background recording and increases the tail.
The graphics path already allocates eight times per frame; recording adds none.
The visible panel allocates, draws and refreshes snapshots, so it is not free.
Resident size can fall when macOS reclaims pages, as it did in panel mode.

Reproduce with `nice -n 10 make -C tests -j2 profiling-render-benchmark
PROFILING_FONT=/absolute/path/to/Gaegu-Bold.ttf`, with the platform's raylib and
Homebrew prefixes if they differ from the Makefile defaults. The panel run
writes `/tmp/afterhours-profiler-benchmark.png` for inspection.

## Sustained recording

A 600,000-frame background run took 63.0 seconds, with p50 0.101417 ms and
p99 0.177250 ms. It made zero C++ allocations per frame. Resident size fell
from 6.23 MB to 4.20 MB, with no net growth. The benchmark preinitializes its
own timing array with nonzero values before the memory sample so writes to
lazy zero pages do not look like collector growth. macOS residency can still
change without an allocation. This is a one-minute soak, not a long-duration
leak guarantee. Collector tests separately enforce history and entry bounds.

Reproduce with `nice -n 10 output/profiling_benchmark_on record 600000` after
building the benchmark. Process metrics remain enabled during the run.


## Per-frame averages

The recent system average covers the same bounded history as the frame chart,
600 completed recording frames by default, configurable with `Options::history`.
The overall system average divides by all completed recording frames since
reset. Both combine repeated calls within a frame and include zero for frames
where the system did not run, including frames before it first appeared.
Stopping discards unfinished-frame samples; resuming continues the recorded
history without counting time spent stopped. Reset clears both averages.

The collector retains one bounded ring of doubles per registered system.
At the default 256-system capacity and 600-frame history, these rings use at
most 1.17 MiB, excluding map entries and names. Rings are allocated when a
system is first seen. Recording completed frames reuses their storage.

A follow-up run with per-system rolling averages used the same 32-system
workload and 3,000 measured frames. Three runs per mode, alternating order,
produced these medians. This run used the layout-only benchmark, with builds
and wm test processes stopped during measurement.

| Mode | Frame p50 ms | Frame p99 ms | C++ allocations/frame | Resident MB | Resident growth MB |
|---|---:|---:|---:|---:|---:|
| Compiled in, stopped | 0.110334 | 0.140250 | 0.00 | 1.64 | 0.00 |
| Background recording | 0.102625 | 0.263916 | 0.00 | 1.83 | 0.00 |
| Recording plus panel construction/layout | 0.223959 | 0.359000 | 29.17 | 2.44 | 0.14 |

Recording still made zero steady-state C++ allocations and showed no resident
growth in these runs. Timing noise is larger than the stopped/recording
difference, so the lower recording median is not evidence of a speedup.
The visible panel still allocates when building UI and refreshing snapshots.
The earlier rendered-panel and sustained-run numbers predate these averages;
they were not rerun for this change.


## Configurable refresh and larger text

The panel now refreshes every rendered frame by default. A positive
`ProfilerOptions::refresh_hz` caps snapshot refreshes. The wm header offers
every frame, 120 Hz, 60 Hz and 30 Hz; the app's rendered frame rate limits each
choice. Collection still happens once per application frame. CPU and resident
memory probes remain capped at two samples per second.

Recent system sums now update while completing each frame, so refreshing a
snapshot no longer rescans every system's history. The visible panel uses
20px text and five rows at 720p. These measurements use that configuration,
three runs per refresh setting in alternating order, with other builds and
wm test processes stopped. Layout runs use 3,000 frames. Rendered runs use
1,500 frames at 1280x720 with Atkinson Hyperlegible and `glFinish`.

| Measurement | Refresh cap | Frame p50 ms | Frame p99 ms | C++ allocations/frame |
|---|---|---:|---:|---:|
| Construction/layout | Every frame | 0.210958 | 0.434167 | 34.00 |
| Construction/layout | 120 Hz | 0.195875 | 0.481458 | 31.07 |
| Construction/layout | 4 Hz | 0.177459 | 0.543875 | 28.85 |
| Rendered panel | Every frame | 1.667792 | 4.775125 | 85.65 |
| Rendered panel | 120 Hz | 1.348791 | 4.651708 | 81.86 |
| Rendered panel | 4 Hz | 1.489416 | 3.938875 | 81.06 |

In this workload every-frame refresh added about 0.034 ms to the median
construction/layout frame and 0.178 ms to the median rendered frame compared
with 4 Hz. Timing varies across runs; the lower 120 Hz rendered median does
not establish a speedup. Background recording still made zero steady-state
C++ allocations and showed zero resident growth in a 3,000-frame check.
The visible panel allocates, and its short rendered runs showed 0.36–0.94 MB
resident growth. These measurements do not establish long-duration memory
stability or guarantee the same cost in an application with different systems.

Reproduce with `profiling_benchmark_on panel 3000 "" RATE` and
`profiling_render_benchmark_on panel 1500 /path/to/AtkinsonHyperlegible-Regular.ttf RATE`,
using 0, 120 and 4 for RATE. Run with `nice -n 10`. The earlier measurements in
this document used the previous UI sizes and the 4 Hz refresh default.
