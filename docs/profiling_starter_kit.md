# Afterhours Profiling Starter Kit (Opt-In)

This starter kit provides reusable performance-testing commands and tooling for
projects using Afterhours.

Nothing here is enabled automatically.

## Design Principle

- No automatic registration
- No automatic overlays
- No project behavior changes unless explicitly enabled

## What is included

- `src/plugins/e2e_testing/perf_commands.h`
  - `dump_profile [count]`
  - `expect_fps_above <fps>`
  - `expect_p99_below <ms>`
- `tools/sample_to_collapsed.py`
  - converts macOS `sample` text to collapsed stacks

## How to enable (explicitly)

```cpp
#include <afterhours/src/plugins/e2e_testing/perf_commands.h>

afterhours::testing::perf_commands::set_provider({
  .get_fps = []() -> std::optional<float> { ... },
  .get_p99_ms = []() -> std::optional<float> { ... },
  .top_entries = [](int n) -> std::vector<afterhours::testing::perf_commands::PerfEntry> { ... },
});

afterhours::testing::perf_commands::register_perf_commands(sm);
```

If this registration call is omitted, no perf commands are installed.

## Minimal project snippet

```cpp
#include <afterhours/src/plugins/e2e_testing/perf_commands.h>

afterhours::testing::perf_commands::set_provider({
  .get_fps = []() -> std::optional<float> {
    int fps = afterhours::graphics::get_fps();
    if (fps <= 0) return std::nullopt;
    return static_cast<float>(fps);
  },
  .get_p99_ms = []() -> std::optional<float> {
    // Return std::nullopt until your project has p99 tracking.
    return std::nullopt;
  },
  .top_entries = [](int) {
    return std::vector<afterhours::testing::perf_commands::PerfEntry>{};
  },
});

afterhours::testing::perf_commands::register_perf_commands(sm);
```

This keeps behavior fully explicit: projects opt in where they register E2E
commands, and can choose partial support while they wire richer metrics.

## Why provider callbacks?

Different projects store profiling data differently. The callback provider keeps
Afterhours generic and avoids forcing one profiler implementation.
