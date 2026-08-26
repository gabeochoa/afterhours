# Windows port: state of play

Short version: **afterhours main already builds for Windows.** The blockers that
downstream projects hit are in their submodule pin, not in this repo. One real
gap remains, and it belongs to the consumers.

## How this was found

Downstream projects were switched to `zig c++`, which can target
`x86_64-windows-gnu` from macOS. Running `make windows` in two of them surfaced
three afterhours errors. All three turned out to be **already fixed on main** —
the projects pin `12a4571`, which predates the fixes.

Do not act on the error list below as if it were open work. It is recorded so
the next person who sees these errors recognises them as a stale-pin symptom.

## Already fixed on main — do not re-fix

| Symptom at pin `12a4571` | State on main |
|---|---|
| `cannot initialize a parameter of type 'const char *' with an rvalue of type 'const value_type *'` — `path.c_str()` is `wchar_t*` on Windows | Fixed. All three `ExportImage` call sites use `path.string().c_str()`: `backends/raylib/windowed.h:123`, `drawing_helpers.h:511`, `headless.h:156` |
| `reference to unresolved using declaration` / `excess elements in scalar initializer` — `std::aligned_alloc` absent in mingw | Fixed. `memory/arena.h:34` has a `_WIN32` branch on `_aligned_malloc`, with a matching `aligned_free_compat` at `:40` so the free side pairs correctly |
| `#error "Headless GL not supported on this platform"`, plus cascading `unknown type name 'HeadlessGL'` | Fixed. `graphics/platform/headless_gl_windows.h` exists and is selected by `headless_gl.h`. An Emscripten stub was added alongside it. The `#error` is now a genuine fallback, not a Windows path |

## The actual action item is downstream

The projects need their submodule pin bumped:

```sh
cd <project>
git -C vendor/afterhours fetch origin
git -C vendor/afterhours checkout origin/main
git add vendor/afterhours && git commit -m "Bump afterhours"
make windows
```

Known to be pinned at `12a4571` and therefore stale: `kart-afterhours`,
`MyNameChef`. Worth checking the rest — `MyNameChef`'s submodule had never even
been initialised.

Expect the bump to be more than a no-op: main is 200+ commits ahead of that pin
and includes behaviour changes (margin/box-model semantics, focus arbitration,
scroll views, `FlexWrap` now defaulting to `NoWrap`). Bump it on its own commit
and run each project's tests before assuming the Windows result is meaningful.

## Still open: the raylib/win32 symbol collision

This one is real and is **not** fixed anywhere. After the pin bump, raylib
projects hit:

```
winuser.h: error: conflicting types for 'CloseWindow'
winuser.h: error: conflicting types for 'ShowCursor'
wingdi.h:  error: declaration conflicts with target of using declaration already in scope
```

raylib declares `CloseWindow`, `ShowCursor` and friends; so does Win32. The
remedy is `#define NOGDI` and `#define NOUSER` (or `WIN32_LEAN_AND_MEAN`) before
anything pulls in `windows.h`. There is currently no `NOGDI`/`NOUSER`/
`WIN32_LEAN_AND_MEAN` anywhere in `src/`.

Open question: fix it per-project, or add a raylib-include shim here that
defines them centrally? A shim is the better trade the moment a second project
needs the same three lines — which, given every raylib consumer will hit this,
is immediately. Sokol projects are unaffected.

## Related

- `~/p/port_to_zig_build.md` — the toolchain migration these findings came from.
- `kart-afterhours/docs/afterhours_gaps.md` — consumer-side notes. Its "Windows
  Cross-Compilation" section describes the three fixed items as open; it is
  stale for the same pin reason.
