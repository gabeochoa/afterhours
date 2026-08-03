# afterhours


an ecs framework based on the one used in https://github.com/gabeochoa/pharmasea

check the examples folder for how to use it :)

## quick start

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


## compiler options: 

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

## Plugins

Plugins are basically just helpful things I added to the library so i can help them in all my projects. They arent needed at all and dont provide examples (yet). All plugins (and any you make i hope) should implement the functions mentioned in developer.h 


### input (requires raylib)
a way to collect user input and map it to specific actions 

Components: 
- InputCollector => where all the action data goes
- ProvidesMaxGamepadID => the total gamepads connected
- ProvidesInputMapping => Stores the mapping from Keys => Actions

Update Systems: 
- InputSystem => does all the heavy lifting

Render Systems:
- RenderConnectedGamepads => renders the number of gamepads connected


### layered_input (requires raylib)
Layered input mapping - different key bindings per game state (e.g., menu vs gameplay).

Components:
- ProvidesLayeredInputMapping<LayerEnum> => Stores per-layer action->key mappings

Update Systems:
- LayeredInputSystem<LayerEnum> => Polls input for the active layer only

Usage:
```cpp
// Define your layer enum
enum class GameLayer { Menu, Playing, Paused };

// Define action enum
enum Action { MoveUp, Confirm, Pause };

// Build mapping
std::map<GameLayer, std::map<int, afterhours::input::ValidInputs>> mapping;
mapping[GameLayer::Menu][Action::MoveUp] = {raylib::KEY_UP};
mapping[GameLayer::Playing][Action::MoveUp] = {raylib::KEY_W};

// Register
afterhours::layered_input<GameLayer>::add_singleton_components(
    entity, mapping, GameLayer::Menu);
afterhours::layered_input<GameLayer>::register_update_systems(systems);

// Switch layers at runtime
auto* mapper = EntityHelper::get_singleton_cmp<
    afterhours::ProvidesLayeredInputMapping<GameLayer>>();
mapper->set_active_layer(GameLayer::Playing);

// Get bindings for current layer
const auto& bindings = mapper->get_bindings(Action::MoveUp);
```


### window_manager (desires raylib)
gives access to resolution and window related functions

Components: 
- ProvidesTargetFPS
- ProvidesCurrentResolution
- ProvidesAvailableWindowResolutions

Update Systems: 
- CollectCurrentResolution => runs when ProvidesCurrentResolution.should_refetch = true
- CollectAvailableResolutions => runs when ProvidesAvailableWindowResolutions.should_refetch = true

Render Systems: 
- :)


### ui (requires magic_enum, desires raylib)
gives access to some UI components 

Components: 
- there are a bunch, but you dont use them directly

Update Systems: 
- register_before_ui_updates() => do this before you run div() or button()
- register_after_ui_updates() => do this after all your ui is run

Render Systems: 
- register_render_systems() => does both UI and debug rendering

UI Elements:
- div, button, slider, dropdown, checkbox, radio group, stepper, toggle switch
- text_input (`with_placeholder`, masking, selection, clipboard, undo/redo)
- tab_container, tree_view, scroll_view, modal, toast
- vsplit / hsplit: divide a region into N parts in one statement
  `auto [title, body, status] = vsplit(ctx, mk(e), {pixels(30), expand(), pixels(50)});`

Styling:
- `ComponentConfig` is the one config struct; `restyle()` applies a partial
  update to an existing element
- `with_styled_label({{"M ", red}, {"file.h", white}})` for multi-colour runs;
  these word-wrap with `TextOverflow::Wrap`
- `theme_io`: read/write a `Theme` as a flat `key = value` text file, plus
  `HotReloadTheme` to re-apply it on save. No JSON dependency.

Scrolling:
- `HasScrollView::scroll_smoothing` — 1.0 (default) snaps, ~0.25 glides. The
  wheel drives `scroll_target` and the rendered offset eases toward it,
  frame-rate independent.

### files
per-app paths and crash-safe persistence.

- `files::init(app_name)`, then `get_config_path()` / `get_save_path()` /
  `get_resource_path(group, name)`. Resource lookup resolves from the
  executable's own location (so a bundled `.app` finds `Contents/Resources`),
  falling back to the working directory for dev builds.
- `files::write_string_atomic(path, content)` writes a sibling temp and renames
  over the target, so a crash mid-write cannot truncate the file the way a
  plain `ofstream` does. `read_string(path)` returns `nullopt` when missing.
  Crash-safe, not power-loss-safe.
- Requires compiling `src/plugins/files.cpp`; the two helpers above are
  header-only and do not.

### texture manager (desires raylib)
sprite rendering

Components: 
- HasSpritesheet 
- HasSprite
- HasAnimation

Update Systems: 
- AnimationUpdateCurrentFrame

Render Systems: 
- RenderSprites
- RenderAnimation


### animation 

- types: `afterhours::animation::EasingType`, `AnimSegment`, `AnimTrack`
- api (templated by your enum key type):
  - `animation::AnimationManager<Key>`: holds tracks
  - `animation::manager<Key>()`: singleton manager accessor
  - `animation::anim(Key key)`: fluent handle with `.from()`, `.to()`, `.sequence()`, `.hold()`, `.on_complete()`, `.on_change()`, `.on_step()`
  - `animation::one_shot(Key key, Fn)` and `animation::one_shot(Enum base, size_t index, Fn)`: run an animation once per key (or per composite key)
  - `AnimHandle::loop_sequence(segments)`: repeat a sequence forever (calls `.sequence` again on complete)
  - `animation::register_update_systems<Key>(SystemManager&)`: updates manager each frame

example:
```cpp
// define your keys (app side)
enum struct UIKey : size_t { MapShuffle };

// wire update
afterhours::animation::register_update_systems<UIKey>(systems);

// start animation
afterhours::animation::anim(UIKey::MapShuffle)
  .from(0.0f)
  .sequence({ { .to_value = 8.f, .duration = 0.45f, .easing = afterhours::animation::animation::EasingType::Linear },
              { .to_value = 5.f, .duration = 0.55f, .easing = afterhours::animation::animation::EasingType::EaseOutQuad } })
  .hold(0.5f)
  .on_step(1.0f, [](int step){ /* called when value crosses multiples of 1.0 */ })
  .on_complete([]{ /* done */ });

// read value in UI
auto v = afterhours::animation::manager<UIKey>().get_value(UIKey::MapShuffle);
```

one-shot usage:
```cpp
// runs once per key
afterhours::animation::one_shot(UIKey::IntroReveal, [](auto h){
  h.from(0.0f).to(1.0f, 0.3f, afterhours::animation::EasingType::EaseOutQuad);
});

// runs once per composite key (enum+index)
afterhours::animation::one_shot(UIKey::MapCard, i, [i](auto h){
  const float delay = 0.05f * static_cast<float>(i);
  h.from(0.0f)
   .sequence({
     { .to_value = 0.0f, .duration = delay,  .easing = afterhours::animation::EasingType::Hold },
     { .to_value = 1.0f, .duration = 0.25f, .easing = afterhours::animation::EasingType::EaseOutQuad },
   });
});
```

looping usage:
```cpp
afterhours::animation::anim(UIKey::Spinner)
  .from(0.0f)
  .loop_sequence({
    { .to_value = 1.0f, .duration = 0.5f, .easing = afterhours::animation::EasingType::Linear },
    { .to_value = 0.0f, .duration = 0.5f, .easing = afterhours::animation::EasingType::Linear },
  });
```



examples in other repos:
- https://github.com/gabeochoa/kart-afterhours/
- https://github.com/gabeochoa/tetr-afterhours/
- https://github.com/gabeochoa/wm-afterhours/
- https://github.com/gabeochoa/ui-afterhours/
- https://github.com/gabeochoa/pong-afterhours/
