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
struct BuildScene : System<UIContext<InputAction>> {
  virtual void for_each_with(Entity &entity, UIContext<InputAction> &context,
                             float) override {
    auto root = imm::div(context, mk(entity),
                         ComponentConfig{}
                             .with_size({percent(1.f), percent(1.f)})
                             .with_flex_direction(FlexDirection::Column)
                             .with_padding(Spacing::lg));
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
  Entity &ui_root = ui::init_ui_plugin<InputAction>();
  ui_root.addComponent<window_manager::ProvidesCurrentResolution>(
      window_manager::Resolution{RENDER_W, RENDER_H});
  EntityHelper::registerSingleton<window_manager::ProvidesCurrentResolution>(
      ui_root);
  ui::UICollectionHolder::get()
      .collection.registerSingleton<window_manager::ProvidesCurrentResolution>(
          ui_root);
  {
    auto &in = EntityHelper::createEntity();
    in.addComponent<input::InputCollector>();
    EntityHelper::registerSingleton<input::InputCollector>(in);
  }
  SystemManager systems;
  ui::enforce_singletons<InputAction>(systems);
  ui::register_before_ui_updates<InputAction>(systems);
  systems.register_update_system(std::make_unique<BuildScene>());
  ui::register_after_ui_updates<InputAction>(systems);
  ui::register_render_systems<InputAction>(systems);
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
