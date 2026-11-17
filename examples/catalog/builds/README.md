# Minimal consumer builds

Three programs, each the smallest supported way to use afterhours for one kind of build. Each one runs to exit 0 and is built by `examples/catalog/run.sh`.

| Example | Backend define | Needs | Display, GL or audio |
|---|---|---|---|
| `pure_ecs` | `-DAFTER_HOURS_USE_NONE` | a C++23 compiler | none |
| `logic_only_ui` | `-DAFTER_HOURS_USE_NONE` | + fmt headers (`brew install fmt` / `libfmt-dev`) | none |
| `offscreen_render` | `-DAFTER_HOURS_USE_RAYLIB` | + raylib (`brew install raylib`, or `pkg-config raylib`) | an offscreen GL context, no window |

```sh
cd examples/catalog/builds/pure_ecs && make
cd examples/catalog/builds/logic_only_ui && make
cd examples/catalog/builds/offscreen_render && make
```

Each makefile is the whole recipe: set `EXTRA_FLAGS` / `LDFLAGS`, then `include ../../common.mk`. `common.mk` adds `-isystem <afterhours>/vendor` (magic_enum) and builds to `output/<name>.exe` (`.exe` on every platform).

## Which one a test suite wants

- **Logic and ECS tests** (`pure_ecs`): the none backend draws nothing and opens nothing, so the test binary runs on a CI box with no display. Entities created outside a system sit in a temp list until `EntityHelper::merge_entity_arrays()` or the end of the first system pass. Merge before the first `run` if frame 1 has to see them.
- **UI interaction and layout tests** (`logic_only_ui`): the none backend plus `ui::setup_with_resolution` runs input, the UI systems and autolayout against a supplied resolution, with deterministic text metrics. Rects are valid after the first `run`.
- **Rendering tests** (`offscreen_render`): `graphics::init(Config{.display = DisplayMode::Headless})` makes an offscreen GL context and `capture_frame(path)` writes a PNG. This needs a GPU driver, unlike the two above. If `init` fails, treat that as a missing capability and fail; don't fall back to the none backend and report a pass.
