// gradient_line_example.cpp
//
// Flat vs gradient fill + imm::line diagonal (R-057). Based on on_draw_example:
//   ComponentConfig::with_on_draw_bg(fn)  -- draws BEHIND the widget's own fill
//   ComponentConfig::with_on_draw_fg(fn)  -- draws ON TOP of all its primitives
//
// The callback type is afterhours::ui::RenderPrimitive::CustomDrawFn, i.e.
//   std::function<void(RectangleType)>.
// It receives the widget's FINAL on-screen rect (after layout) and draws with
// the raw afterhours::draw_* primitives. Both callbacks fire at the widget's
// own render layer, so ordering is exactly: bg -> widget's fill/border -> fg.
//
// ---------------------------------------------------------------------------
// WHY THIS BEATS THE OLD PATTERN
// ---------------------------------------------------------------------------
// Before this feature, drawing a custom backdrop/overlay on a widget meant:
//   1. Tag the widget with a debug name.
//   2. Elsewhere, run a separate BG render system that did ls_rect("that-name")
//      to look the widget's rect back up, then drew behind it.
//   3. Run yet another separate FG render system to draw on top.
// That scattered one widget's visuals across three places, coupled them by a
// stringly-typed name, and made ordering depend on system registration order.
//
// With with_on_draw_bg/fg the custom visual lives INLINE on the widget config,
// no name lookup, no extra systems, no coupling. Read it where you build it.
//
// Build: cd examples && make on_draw_example
// Run:   ./gradient_line_example      (writes gradient_line_example.png, exits 0)

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <cassert>
#include <cstdio>
#include <filesystem>

#include <afterhours/ah.h>
#include <afterhours/src/drawing_helpers.h>
#include <afterhours/src/graphics.h>
#define AFTER_HOURS_IMM_UI
#include <afterhours/src/plugins/ui.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

// The imm nav/text systems key off an InputAction enum. We don't wire any real
// input here, but the vocabulary must exist for the plugin to instantiate.
enum struct InputAction {
  None,
  WidgetMod,
  WidgetNext,
  WidgetBack,
  WidgetPress,
  WidgetLeft,
  WidgetRight,
  WidgetUp,
  WidgetDown,
  MenuBack,
};

static constexpr int RENDER_W = 660;
static constexpr int RENDER_H = 320;

// Builds the whole scene each frame. In imm UI you (re)declare your widgets
// every frame; the config carries the on_draw callbacks along for the ride.
struct BuildScene : System<UIContext<InputAction>> {
  virtual void for_each_with(Entity &entity, UIContext<InputAction> &context,
                             float) override {
    // Root column that fills the window.
    auto root = imm::div(context, mk(entity),
                         ComponentConfig{}
                             .with_size({percent(1.f), percent(1.f)})
                             .with_flex_direction(FlexDirection::Column)
                             .with_padding(Spacing::lg));

    // Left: flat fill. Right: gradient fill + diagonal line (R-057).
    auto row = imm::div(context, mk(root.ent()),
                        ComponentConfig{}
                            .with_flex_direction(FlexDirection::Row)
                            .with_gap(pixels(24.f)));
    imm::div(context, mk(row.ent(), 0),
             ComponentConfig{}
                 .with_size({pixels(180.f), pixels(140.f)})
                 .with_custom_background(Color{90, 90, 100, 255})
                 .with_alignment(TextAlignment::Center)
                 .with_label("flat"));
    imm::div(context, mk(row.ent(), 1),
             ComponentConfig{}
                 .with_size({pixels(180.f), pixels(140.f)})
                 .with_gradient(Color{40, 90, 220, 255}, Color{180, 60, 200, 255})
                 .with_alignment(TextAlignment::Center)
                 .with_label("gradient"));
    imm::line(context, mk(row.ent(), 2), Vector2Type{0.f, 1.f},
              Vector2Type{1.f, 0.f}, Color{255, 220, 60, 255}, 4.f,
              ComponentConfig{}.with_size({pixels(180.f), pixels(140.f)}));
  }
};

int main() {
  // 1. Bring up a hidden windowed backend so there's a real GL context to
  //    render + capture through (no window pops up).
  graphics::Config cfg{};
  cfg.display = graphics::DisplayMode::Windowed;
  cfg.width = RENDER_W;
  cfg.height = RENDER_H;
  cfg.target_fps = 60;
  cfg.config_flags = raylib::FLAG_WINDOW_HIDDEN;
  cfg.enable_msaa = false;
  if (!graphics::init(cfg)) {
    fprintf(stderr, "FATAL: windowed backend init failed\n");
    return 1;
  }

  // 2. Create the imm UI collection + singletons (UIContext, FontManager).
  //    Must run after graphics::init since fonts need a GL context.
  Entity &ui_root = ui::init_ui_plugin<InputAction>();
  ui_root.addComponent<window_manager::ProvidesCurrentResolution>(
      window_manager::Resolution{RENDER_W, RENDER_H});
  EntityHelper::registerSingleton<window_manager::ProvidesCurrentResolution>(
      ui_root);
  ui::UICollectionHolder::get()
      .collection.registerSingleton<window_manager::ProvidesCurrentResolution>(
          ui_root);

  // The bundled debug-overlay system reads inputs; give it an empty collector
  // so its should_run check doesn't dereference an empty optional.
  {
    auto &in = EntityHelper::createEntity();
    in.addComponent<input::InputCollector>();
    EntityHelper::registerSingleton<input::InputCollector>(in);
  }

  // 3. Canonical imm wiring:
  //    before-bridge (Clear/Begin) -> build -> after-bridge (End) -> render.
  SystemManager systems;
  ui::enforce_singletons<InputAction>(systems);
  ui::register_before_ui_updates<InputAction>(systems);
  systems.register_update_system(std::make_unique<BuildScene>());
  ui::register_after_ui_updates<InputAction>(systems);
  ui::register_render_systems<InputAction>(systems);

  // 4. Render one frame and capture it.
  graphics::begin_frame();
  raylib::ClearBackground(raylib::Color{25, 25, 32, 255});
  systems.run(1.f / 60.f);
  graphics::end_frame();

  std::filesystem::path out = "gradient_line_example.png";
  bool ok = graphics::capture_frame(out);
  printf("screenshot: %s -> %s\n", ok ? "OK" : "FAILED", out.c_str());

  graphics::shutdown();
  return ok ? 0 : 1;
}
