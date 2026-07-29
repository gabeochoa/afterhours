// on_draw_example.cpp
//
// A tiny, pedagogical demo of the per-widget custom-draw callbacks:
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
// Run:   ./on_draw_example      (writes on_draw_example.png, exits 0)

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

static constexpr int RENDER_W = 480;
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

    // One panel that shows BOTH callbacks at once.
    //
    // - with_custom_background paints the panel's OWN fill (a light gray).
    // - with_on_draw_bg runs FIRST, behind that fill. We draw a larger rect
    //   inset outward by 16px so a colored border "halo" peeks out around the
    //   panel, proving the bg draw is truly behind the fill.
    // - with_on_draw_fg runs LAST, on top of everything. We draw green corner
    //   brackets that sit visibly over the gray fill, proving fg is on top.
    imm::div(
        context, mk(root.ent()),
        ComponentConfig{}
            .with_size({pixels(300.f), pixels(160.f)})
            .with_margin(Spacing::md)
            .with_custom_background(Color{235, 235, 235, 255})
            // BG: a colored backdrop that peeks out behind the panel fill.
            .with_on_draw_bg([](RectangleType r) {
              draw_rectangle(RectangleType{r.x - 16, r.y - 16, r.width + 32,
                                           r.height + 32},
                             Color{70, 110, 200, 255});
            })
            // FG: corner brackets drawn on top of the panel fill.
            .with_on_draw_fg([](RectangleType r) {
              const float len = 28.f, thick = 5.f;
              const Color g{60, 200, 90, 255};
              // top-left
              draw_rectangle(RectangleType{r.x, r.y, len, thick}, g);
              draw_rectangle(RectangleType{r.x, r.y, thick, len}, g);
              // bottom-right
              draw_rectangle(RectangleType{r.x + r.width - len,
                                           r.y + r.height - thick, len, thick},
                             g);
              draw_rectangle(RectangleType{r.x + r.width - thick,
                                           r.y + r.height - len, thick, len},
                             g);
            }));
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

  std::filesystem::path out = "on_draw_example.png";
  bool ok = graphics::capture_frame(out);
  printf("screenshot: %s -> %s\n", ok ? "OK" : "FAILED", out.c_str());

  graphics::shutdown();
  return ok ? 0 : 1;
}
