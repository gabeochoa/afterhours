# Profiling Starter Snippet (Template-Friendly)

Use this snippet in a new Afterhours project to opt into perf commands.

Nothing is automatic: you must call both `set_provider(...)` and
`register_perf_commands(...)`.

```cpp
#include <afterhours/src/plugins/e2e_testing/perf_commands.h>

inline void register_project_e2e_commands(afterhours::SystemManager &sm) {
  using namespace afterhours::testing;

  perf_commands::set_provider({
      .get_fps = []() -> std::optional<float> {
        // Replace with your real FPS source when available.
        return std::nullopt;
      },
      .get_p99_ms = []() -> std::optional<float> {
        // Replace with your real p99 source when available.
        return std::nullopt;
      },
      .top_entries = [](int) -> std::vector<perf_commands::PerfEntry> {
        // Replace with your project profiler source.
        return {};
      },
  });

  // Explicit opt-in registration.
  perf_commands::register_perf_commands(sm);
}
```

## Commands enabled

- `dump_profile [count]`
- `expect_fps_above <fps>`
- `expect_p99_below <ms>`

## Bring-up order

1. Wire `dump_profile` first (`top_entries` callback).
2. Wire FPS metric (`get_fps`).
3. Wire p99 metric (`get_p99_ms`).
4. Add deterministic scenario in your project tests.
