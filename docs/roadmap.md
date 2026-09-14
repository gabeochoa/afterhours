# Architecture research

Historical proposals consolidated September 2026. Original plans remain in Git at
`c93e10e`; use `git show c93e10e:docs/plans/<name>.md`. These are design directions,
not fresh missing-feature claims, measured gains or implementation approval.
Current follow-ups are in [todo.md](../todo.md).

## Performance

Measure first, preserve public behavior, and compare allocations, frame percentiles,
CPU and memory. Prioritize repeated work before changing storage architecture.

| Area | Candidates and constraints |
|---|---|
| ECS | Skip empty merges/system scans; singleton lookup; avoid materialized first-result queries and unnecessary sort. Preserve ordered/take query semantics. |
| Entity storage | Pool entities with complete reset and generation checks. SoA/archetypes and component indexes require caller/performance evidence. Keep transient pointer lifetime explicit. |
| UI layout | Reuse scratch buffers, reduce maps/callback allocations, reconcile sizing and placement. Skip scroll-only layout only with complete invalidation. |
| Text | Share measurement inputs and retained layout; cache font generations, not reused handles alone. See atlas measurement for the completed narrow Sokol fix. |
| Collision | Cache candidates/masses, skip infinite-mass pairs, evaluate a spatial hash. Preserve collision ordering and correctness. |
| Pathfinding | Reuse search buffers, share immutable grids, bound worker queues and cancellation. |
| Input | Cache frame samples/singletons; avoid changing press/hold semantics. |
| Animation | Avoid idle manager work; separate interpolation values from scheduling. |
| Audio/textures | Cache lookups and own resource lifetime. Atlas packing does not prove draw reduction. |
| Timers/camera | Optimize only measured hot loops; preserve dt, catch-up and transform behavior. |

Older plans compared Clay, RAD Debugger and Large Arrays of Things. Intrusive UI
links, nil sentinels, slot free lists and lightweight hot-object pools are alternatives,
not prerequisites. Do not replace extensible ECS components with a universal fat struct
or fixed-size global array merely to resemble another implementation.

## Reusable styling and components

Presets are value-based ComponentConfig bundles. Recipes may add named widgets and
variants; composition helpers should remove repeated code without hiding ownership.
Define precedence among theme, recipe, preset and explicit call-site settings before
adding a registry. Keep runtime styling typed; no external stylesheet or mandatory
virtual widget hierarchy was proposed. Benchmark transition/state bookkeeping.

PanGui and bazza/ui supplied examples for state overrides, hover/press transitions,
enter/exit animation, sheets and anchored popovers. Exit animation needs deliberate
retention and teardown; a removed immediate widget cannot animate itself. Existing
widgets must be checked before treating an old gap as missing.

## Debugging and renderer verification

`ui_debug_mode.h` provides grid, render flash, dimensions, hover inspection and a panel.
Keep it optional. Loupe inspired the tooling; diagnostics should not change normal layout.

Compare windowed/headless and Raylib/Sokol using separate processes and shared scenes.
Check geometry, advances and pixels separately; backend rasterization can differ.
Include translucent colors, text/fallback, clipping, transforms, MSAA and HiDPI.
Avoid repeated backend initialization in one process where lifetime is unsupported.
Measured layout and rendered glyphs must use the same font contract.

## Hit targets and accessibility settings

A static hit region may differ from visual bounds. Share one resolved hit rectangle
across pointer picking, hover and validation; define overlap priority, clipping and
anchor semantics. Predictive/dynamic target expansion remains research.

The accessible-settings RFC proposed typed metadata, normal UI controls, live preview
with Apply/Cancel, atomic persistence, safe display-mode rollback, localization and
optional speech. Keep semantics separate from platform adapters. Pointer-only and
keyboard-only completion, reduced motion, focus restoration and translated layouts
need runtime checks; metadata alone cannot guarantee accessibility.

Open decisions include schema/versioning, rollback after external changes, speech
provider ownership, platform availability and adoption cost. Start with a real consumer.

## Persisted relationships

Store IDs/handles in snapshots rather than pointer graphs. Test stale handles, undo,
recreation, migration and frame-lifetime guarantees before changing allocation.
Component pools are a separate performance decision. Ownership/cascade deletion needs
explicit non-owning links and cycle behavior; it is not implemented by this proposal.

## Windows notes

Older consumers at `12a4571` lacked path conversion, aligned allocation/free pairing
and headless Windows GL support that later landed. Recheck pins and run consumer tests
before a bump. The old Raylib/Win32 CloseWindow/ShowCursor name-collision report still
needs a current reproduction and ownership decision; suppressing Windows declarations
must not silently remove APIs consumers need. Sokol uses a different path.
