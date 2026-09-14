# Profiling measurements

September 12, 2026, Apple M4/macOS, clang C++20 -O2, niceness 10. A workload runs
32 systems with 100 sine operations each after 200 warmup frames. Alternate run
order and keep builds out of measurement. Counts cover C++ new, not all platform
allocations; short RSS samples do not establish leak freedom.

## Initial collector, before rolling system averages

Five runs per mode, 3,000 frames; medians:

| Mode | Frame p50 / p99 ms | Allocations/frame | Resident growth MB |
|---|---:|---:|---:|
| Compiled out | .079750 / .087625 | 0 | 0 |
| Stopped | .080000 / .087083 | 0 | 0 |
| Recording | .088209 / .098500 | 0 | 0 |
| Panel construction/layout | .200458 / .237250 | 31.01 | .11 |

Recording added about 8.5us in this fixture. Panel layout excluded drawing and used
4Hz refresh. Three real-renderer repeats at 1280×720 with Gaegu Bold and glFinish
measured recording p50 .739166ms versus panel 1.613834ms; p99 1.942834 versus 5.086834ms.
The renderer already allocated eight times/frame; the panel averaged 81.22.
GPU completion is included, window presentation is not; harness text metrics were fixed.

A 600,000-frame recording soak took 63s, allocated zero times/frame and showed no net
RSS growth. This predates rolling system averages and is not a long-duration guarantee.

## Rolling system averages

Each system retains a bounded double ring, up to 1.17MiB for 256×600 entries plus names/maps.
Three alternating layout-only runs measured recording p50 .102625ms, panel .223959ms;
allocations/frame 0 versus 29.17. Recording showed no RSS growth. Timing noise exceeded
the stopped/recording difference; it is not evidence of a recording speedup.
The earlier render/soak numbers were not rerun for this change.

## Current refresh options and larger type

Three alternating runs per setting; layout 3,000 frames, rendered 1,500 frames at
1280×720 with Atkinson Hyperlegible, 20px text, five rows and glFinish:

| Mode | Refresh | p50 / p99 ms | Allocations/frame |
|---|---|---:|---:|
| Layout | Every frame | .210958 / .434167 | 34.00 |
| Layout | 120Hz | .195875 / .481458 | 31.07 |
| Layout | 4Hz | .177459 / .543875 | 28.85 |
| Rendered | Every frame | 1.667792 / 4.775125 | 85.65 |
| Rendered | 120Hz | 1.348791 / 4.651708 | 81.86 |
| Rendered | 4Hz | 1.489416 / 3.938875 | 81.06 |

Every-frame refresh added .034ms layout and .178ms rendered median versus 4Hz here.
The lower 120Hz rendered median is noise, not a general speedup. Background recording
still allocated zero times/frame in a 3,000-frame check. Visible-panel RSS grew .36–.94MB;
these short runs do not establish long-term stability or costs for another application.

After building with the [profiling targets](profiling.md), repeat with niceness 10:
`profiling_benchmark_on panel 3000 "" RATE` or
`profiling_render_benchmark_on panel 1500 /path/to/AtkinsonHyperlegible-Regular.ttf RATE`.
Use RATE 0, 120 and 4. `profiling_benchmark_on record 600000` repeats the soak.
Remove the task-owned `/tmp/afterhours-profiler-benchmark.png` after inspection.
