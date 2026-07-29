// imm_visual_check.cpp
// Visual regression for two imm rendering fixes, rendered through the REAL imm
// system (hidden window -> captured PNG):
//   1. HasTexture honors with_opacity() — the right textured tile fades.
//   2. A custom-background button's hover derives a tint of its OWN color
//      (hover_bg), instead of flashing to the theme Background. The right
//      button is forced "hot" so the hover fill is exercised.
//
// Build: cd examples && make imm_visual_check
// Run:   ./imm_visual_check   (writes imm_visual_check.png, asserts hover_bg)

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <string>

#include <afterhours/ah.h>
#include <afterhours/src/drawing_helpers.h>
#include <afterhours/src/graphics.h>
#define AFTER_HOURS_IMM_UI
#include <afterhours/src/plugins/ui.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

// Full vocabulary the imm nav/text/debug systems reference.
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
  TextBackspace,
  TextCopy,
  TextCut,
  TextDelete,
  TextDeleteWordBack,
  TextDeleteWordForward,
  TextEnd,
  TextHome,
  TextPaste,
  TextRedo,
  TextSelectAll,
  TextSelectLeft,
  TextSelectRight,
  TextUndo,
  TextWordLeft,
  TextWordRight,
};

static constexpr int RENDER_W = 640;
static constexpr int RENDER_H = 480;

// Populated during the build pass so main() can force the hover state and the
// texture is available when the config is assembled.
static texture_manager::Texture g_tex{};
static EntityID g_hover_button_id = -1;

// with_on_draw invocation counters (fix: custom-draw callbacks).
static int g_bg_calls = 0;
static int g_fg_calls = 0;

struct BuildScene : System<UIContext<InputAction>> {
  virtual void for_each_with(Entity &entity, UIContext<InputAction> &context,
                             float) override {
    const Color teal{60, 150, 150, 255};

    auto col = imm::div(context, mk(entity),
                        ComponentConfig{}
                            .with_size({percent(1.f), percent(1.f)})
                            .with_flex_direction(FlexDirection::Column)
                            .with_padding(Spacing::md));

    // Row 1: textured tiles — left full opacity, right faded (fix #1).
    auto tex_row = imm::div(context, mk(col.ent()),
                            ComponentConfig{}
                                .with_size({percent(1.f), pixels(140.f)})
                                .with_flex_direction(FlexDirection::Row));
    imm::div(context, mk(tex_row.ent()),
             ComponentConfig{}
                 .with_size({pixels(140.f), pixels(140.f)})
                 .with_margin(Spacing::sm)
                 .with_texture(g_tex));
    imm::div(context, mk(tex_row.ent()),
             ComponentConfig{}
                 .with_size({pixels(140.f), pixels(140.f)})
                 .with_margin(Spacing::sm)
                 .with_texture(g_tex)
                 .with_opacity(0.35f));

    // Row 2: custom-bg buttons — left at rest, right forced hot (fix #2).
    auto btn_row = imm::div(context, mk(col.ent()),
                            ComponentConfig{}
                                .with_size({percent(1.f), pixels(60.f)})
                                .with_flex_direction(FlexDirection::Row));
    imm::button(context, mk(btn_row.ent()),
                ComponentConfig{}
                    .with_size({pixels(180.f), pixels(50.f)})
                    .with_margin(Spacing::sm)
                    .with_custom_background(teal)
                    .with_label("at rest"));
    auto hot = imm::button(context, mk(btn_row.ent()),
                           ComponentConfig{}
                               .with_size({pixels(180.f), pixels(50.f)})
                               .with_margin(Spacing::sm)
                               .with_custom_background(teal)
                               .with_label("hovered"));
    g_hover_button_id = hot.id();

    // Row 3: with_on_draw. White widget; bg draws a red rect expanded 20px
    // (peeks out behind the white fill -> proves "behind"); fg draws green
    // corner brackets (visible on top of white -> proves "on top").
    auto od_row = imm::div(context, mk(col.ent()),
                           ComponentConfig{}
                               .with_size({percent(1.f), pixels(140.f)})
                               .with_flex_direction(FlexDirection::Row));
    imm::div(context, mk(od_row.ent()),
             ComponentConfig{}
                 .with_size({pixels(200.f), pixels(100.f)})
                 .with_margin(Spacing::md)
                 .with_custom_background(Color{240, 240, 240, 255})
                 .with_on_draw_bg([](RectangleType r) {
                   g_bg_calls++;
                   draw_rectangle(RectangleType{r.x - 20, r.y - 20,
                                                r.width + 40, r.height + 40},
                                  Color{220, 60, 60, 255});
                 })
                 .with_on_draw_fg([](RectangleType r) {
                   g_fg_calls++;
                   const float s = 26.f, t = 6.f;
                   const Color g{60, 200, 90, 255};
                   draw_rectangle(RectangleType{r.x, r.y, s, t}, g);
                   draw_rectangle(RectangleType{r.x, r.y, t, s}, g);
                   draw_rectangle(
                       RectangleType{r.x + r.width - s, r.y + r.height - t, s, t},
                       g);
                   draw_rectangle(
                       RectangleType{r.x + r.width - t, r.y + r.height - s, t, s},
                       g);
                 }));
  }
};

// Runs after the build pass; forces the "hovered" button hot so RenderImm
// exercises the hover-fill (hover_bg) path.
struct ForceHover : System<UIContext<InputAction>> {
  virtual void for_each_with(Entity &, UIContext<InputAction> &context,
                             float) override {
    context.set_hot(g_hover_button_id);
  }
};

// hover_bg() logic check (the non-trivial part of fix #2).
static void assert_hover_bg() {
  const Color dark{60, 150, 150, 255};
  assert(colors::luminance(HasColor{dark}.hover_bg()) > colors::luminance(dark)
         && "dark base should hover lighter");
  const Color light{230, 230, 230, 255};
  assert(colors::luminance(HasColor{light}.hover_bg()) < colors::luminance(light)
         && "light base should hover darker");
  Color base{10, 20, 30, 255};
  Color override{200, 50, 50, 255};
  HasColor hc{base};
  hc.hover_color = override;
  const Color got = hc.hover_bg();
  assert(got.r == override.r && got.g == override.g && got.b == override.b
         && "explicit hover_color must win");
  printf("hover_bg assertions: PASS\n");
}

int main(int argc, char **argv) {
  assert_hover_bg();

  // Render path: default is the immediate RenderImm; pass "batched" for the
  // RenderBatched path (the one the game actually uses). Both must honor
  // with_on_draw / opacity / hover_bg.
  bool batched = argc > 1 && std::string(argv[1]) == "batched";
  const char *out_name =
      batched ? "imm_visual_check_batched.png" : "imm_visual_check.png";

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

  // Creates the UI collection + singletons (UIContext, FontManager w/ fonts).
  // Must run after graphics::init (get_default_font needs a GL context).
  Entity &ui_root = ui::init_ui_plugin<InputAction>();
  ui_root.addComponent<window_manager::ProvidesCurrentResolution>(
      window_manager::Resolution{RENDER_W, RENDER_H});
  EntityHelper::registerSingleton<window_manager::ProvidesCurrentResolution>(
      ui_root);
  ui::UICollectionHolder::get()
      .collection.registerSingleton<window_manager::ProvidesCurrentResolution>(
          ui_root);

  // Empty input collector so the bundled debug-overlay render system's
  // should_run (which reads inputs) doesn't hit an empty optional. No mapping
  // needed — with no inputs the debug toggle stays off.
  {
    auto &in = EntityHelper::createEntity();
    in.addComponent<input::InputCollector>();
    EntityHelper::registerSingleton<input::InputCollector>(in);
  }

  raylib::Image img = raylib::GenImageChecked(
      128, 128, 16, 16, raylib::Color{80, 200, 240, 255},
      raylib::Color{240, 120, 80, 255});
  g_tex = raylib::LoadTextureFromImage(img);
  raylib::UnloadImage(img);

  // Canonical imm wiring: before-bridge (Clear/Begin) -> build -> after-bridge
  // (End) -> ForceHover -> render-bridge (RenderImm). Bridges drive the imm
  // systems on the UI collection.
  SystemManager systems;
  ui::enforce_singletons<InputAction>(systems);
  ui::register_before_ui_updates<InputAction>(systems);
  systems.register_update_system(std::make_unique<BuildScene>());
  ui::register_after_ui_updates<InputAction>(systems);
  systems.register_update_system(std::make_unique<ForceHover>());
  if (batched)
    ui::register_batched_render_systems<InputAction>(systems);
  else
    ui::register_render_systems<InputAction>(systems);

  graphics::begin_frame();
  raylib::ClearBackground(raylib::Color{20, 20, 28, 255});
  systems.run(1.f / 60.f);
  graphics::end_frame();

  std::filesystem::path out = out_name;
  bool ok = graphics::capture_frame(out);
  printf("[%s] screenshot: %s -> %s\n", batched ? "batched" : "immediate",
         ok ? "OK" : "FAILED", out.c_str());

  // with_on_draw callbacks must have fired (also guards the render-eligibility
  // check for on_draw-only widgets).
  printf("on_draw calls: bg=%d fg=%d\n", g_bg_calls, g_fg_calls);
  assert(g_bg_calls > 0 && "with_on_draw_bg must fire");
  assert(g_fg_calls > 0 && "with_on_draw_fg must fire");

  raylib::UnloadTexture(g_tex);
  graphics::shutdown();
  return ok ? 0 : 1;
}
