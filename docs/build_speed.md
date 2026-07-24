# Build speed notes

Clean-build cost in this header-only lib is dominated by heavy stdlib headers
pulled into every translation unit. Measured per-TU parse cost (clang, -std=c++20):

| header        | parse cost |
|---------------|-----------|
| `<format>`    | ~0.42s    |
| `<iostream>`  | ~0.45s    |
| `<sstream>`   | ~0.50s    |

`logging.h` is included by ~44 headers (so effectively every consumer TU) and was
pulling both `<format>` and `<iostream>`. `<format>` is the single biggest cost in
the ECS-core include chain, and it comes *only* from `logging.h`.

## Changes here (no new .cpp files, API unchanged by default)

1. `logging.h` / `developer.h`: `<iostream>` -> `<cstdio>` (`std::fprintf`). Drops the
   iostream parse from two of the most-included headers. Behavior identical.
2. `logging.h`: `<format>` is now included only in the default logging path, and a new
   opt-in `AFTER_HOURS_LEAN_LOGGING` stubs the `log_*` functions so `<format>` is skipped
   entirely for builds that don't need formatted log output.

Measured on a lean ECS-core TU: ~0.76s -> ~0.63s (~17%) with `AFTER_HOURS_LEAN_LOGGING`.
Multiplied across a full multi-TU build this is real wall-clock time.

## Further ideas (not done here)
- Split the largest plugin headers (`autolayout.h` 1.5k, `input_system.h` 1.3k) so a
  consumer only pays for what it uses.
- Precompiled header for `ah.h` in consumer build systems.
