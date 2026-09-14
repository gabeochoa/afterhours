# afterhours

A C++ ECS framework with optional UI, input, graphics and platform plugins.

## Quick start

A window, an event loop, a clickable button and a hot-reloading theme file:

```cpp
#include "ah.h"
#include "src/graphics.h"
#define AFTER_HOURS_IMM_UI
#include "src/plugins/ui.h"

using namespace afterhours;
using namespace afterhours::ui;

struct Hello : System<DefaultUIContext> {
  void for_each_with(Entity &entity, DefaultUIContext &ctx, float) override {
    if (imm::button(ctx, imm::mk(entity), "Hello World!"))
      printf("clicked\n");
  }
};

int main() {
  return ui::run<>({.title = "hello"},
                   std::make_unique<theme_io::HotReloadTheme>("hello.theme"),
                   std::make_unique<Hello>());
}
```

`ui::run()` owns window setup, the default keymap, system registration, the
frame loop and teardown. See `examples/catalog/ui/hello`; the catalog has ~50
runnable examples.

## Compiler options

AFTER_HOURS_MAX_COMPONENTS
- sets how big the bitset is for components, defaults to 128

AFTER_HOURS_USE_RAYLIB
- some of the plugins use raylib functions (like raylib::SetWindowSize or raylib::IsKeyPressed). Not used for main library 

AFTER_HOURS_USE_METAL
- selects the sokol/Metal backend instead of raylib. Needs one Objective-C++
  translation unit defining SOKOL_IMPL plus capture_impl.h; see
  examples/web/sokol_impl.cc for the canonical one. Supports windowed and
  headless (offscreen) rendering; headless honours Config.hidpi to supersample
  captures.

AFTER_HOURS_IMM_UI
- enables the immediate-mode UI helpers (div/button/slider/...)

AFTER_HOURS_INCLUDE_DERIVED_CHILDREN
- Allows access to for_each_with_derived which will return all entities which match a component or a components children (TODO add an example) 

AFTER_HOURS_REPLACE_LOGGING
- if you want the library to log, implement the four functions and define this

AFTER_HOURS_REPLACE_VALIDATE
- same as logging but assert + log_error

AFTER_HOURS_DEBUG
- enables some debug logging

AFTER_HOURS_INPUT_VALIDATION_<>
- for UI plugin, validates that you have mapped input actions used by the plugin
- add ASSERT or LOG_ONLY to enable validation
- or NONE (default) to disable it

AFTER_HOURS_ENTITY_ALLOC_DEBUG
- turns on log_warn whenever Entities deallocates (and theoretically allocates but unlikely) 

AFTER_HOURS_ENABLE_MCP
- enables the MCP (Model Context Protocol) server for external tool integration
- allows screenshot capture, input injection, UI tree inspection via JSON-RPC
- useful for automated testing and AI-assisted development

AFTERHOURS_ENFORCE_MIN_FONT_SIZE
- enables minimum accessible font size enforcement in the UI plugin
- logs a warning when font sizes are below TypographyScale::MIN_ACCESSIBLE_SIZE_720P (18.67px)
- automatically clamps font sizes to the minimum to ensure accessibility

## Packaging a desktop app (macOS)

`tools/mk_bundle.sh` packages a built executable as a `.app`. Nothing in the
library calls it, so it costs you nothing unless you do. Two lines of make:

```make
bundle: $(EXE)
	@vendor/afterhours/tools/mk_bundle.sh --exe $(EXE) \
	    --name MyApp --id com.example.myapp --resources output/resources
```

`--help` lists everything. The common flags are `--version`, `--icon`,
`--category`, `--url-scheme` (repeatable), and `--sign -` for ad-hoc signing.
Anything not modelled goes in verbatim with `--plist-extra FILE`, so the script
does not need a flag per plist key.

Two details it handles that are easy to get wrong by hand: `CFBundleExecutable`
is derived from the copied binary's filename (a mismatch produces a bundle that
silently refuses to launch), and `NSHighResolutionCapable` is always set
(without it the window is upscaled from 1x and looks soft on Retina). The
generated plist is checked with `plutil -lint` before the script exits.

This pairs with the files plugin: a `.app` puts the binary in
`Contents/MacOS`, which is what `files::get_resource_path` keys on to find
`Contents/Resources`. Pass `--resources` and a bundled app finds its own files.

Linux `.desktop` and Windows packaging are not implemented; `--platform` errors
for them rather than producing something untested.

## Web (Emscripten / raylib)

`tools/web.mk` + `tools/web/shell.html` package a raylib game as
`index.html` / `.js` / `.wasm` / `.data`. Opt-in. include the makefile when you
want `make web`:

```make
WEB_NAME := MyGame
WEB_VERSION := 0.1.0
WEB_TITLE := My Game
WEB_SRCS := $(SRC_FILES) vendor/afterhours/src/plugins/files.cpp
WEB_CXXFLAGS := -std=c++23 -O2 -DNDEBUG -DPLATFORM_WEB \
    -DAFTER_HOURS_USE_RAYLIB $(INCLUDES) ...
OBJ_DIR := ./output
RAYLIB_WEB_SRC := /path/to/raylib   # source tree; builds PLATFORM_WEB .a
include vendor/afterhours/tools/web.mk
```

Prerequisites: Emscripten SDK on PATH (or at `EMSDK`, default `F:/emsdk` with
MSYS path fix), and a raylib **source** checkout for `make web-raylib`.

Gotchas encoded in the flags (re-test audio if you change them):

- **No `ALLOW_MEMORY_GROWTH`**. miniaudio's `ScriptProcessorNode` caches
  `HEAPF32` views; growth detaches them and crashes `onaudioprocess`.
- **Export `HEAPF32`** (and `ccall`) via `EXPORTED_RUNTIME_METHODS`.
- **`ASYNCIFY`** so a normal `while (!WindowShouldClose())` main loop works.
- **Fullscreen** needs a user gesture. Prefer
  `graphics::web_request_fullscreen_now()` from a click handler, or
  `graphics::web_apply_fullscreen(want)` which arms `Module.pendingFullscreen`
  for the shell's next click/key. Do not rely on raylib's deferred
  `ToggleFullscreen` alone on web.

Runtime helpers (available via `graphics.h` only when `__EMSCRIPTEN__` is
defined; no-ops are unnecessary because desktop never includes `web.h`):

- `files::chdir_to_resource_root()`. packaged exe next to `resources/`
- `graphics::web_fit_canvas_to_browser()`. size to `innerWidth`/`innerHeight`
- `graphics::web_apply_fullscreen` / `web_request_fullscreen_now` /
  `web_exit_fullscreen` / `web_is_fullscreen`

The sokol demo under `examples/web/` is separate (Metal/WebGL2 via sokol_app);
use `tools/web.mk` for raylib games.

## behaviour changes worth knowing

- **`FlexWrap` now defaults to `NoWrap`.** It used to be `Wrap`, which meant a
  Column taller than its viewport silently wrapped its children into a second
  column off-screen instead of overflowing. Ask for the old behaviour with
  `with_wrap()`.
- **The sokol backend now alpha-blends.** `sgl_defaults()` loads a pipeline with
  blending off, so low-alpha colours used to fill fully opaque and transparent
  texture texels blitted as black. Opaque drawing is unchanged.
- **`with_font_size()` works on its own.** It used to be ignored unless you also
  named a font, which quietly disabled text wrapping.

## Reference

- [Plugin authoring](PLUGIN_API.md) and [UI](src/plugins/ui/README.md)
- [Optional APIs](docs/plugins.md) and [profiling](docs/profiling.md)
- [Measurements](docs/profiling-measurements.md) and [text measurement](docs/font-atlas-measurement.md)
- [Backlog](todo.md), [research](docs/roadmap.md), and [changelog](CHANGELOG.md)

## License

MIT, see `LICENSE`. The vendored libraries under `vendor/` keep their own
licenses (MIT, zlib, BSL-1.0, public domain); notices are retained in-header.

examples in other repos:
- https://github.com/gabeochoa/kart-afterhours/
- https://github.com/gabeochoa/tetr-afterhours/
- https://github.com/gabeochoa/wm-afterhours/
- https://github.com/gabeochoa/ui-afterhours/
- https://github.com/gabeochoa/pong-afterhours/
