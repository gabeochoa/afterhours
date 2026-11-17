#include <cstdio>

#include "../../../../ah.h"
#include "../../../../src/graphics.h"
#define AFTER_HOURS_IMM_UI
#include "../../../../src/plugins/e2e_testing/platform_test_input.h"
#include "../../../../src/plugins/ui.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

struct MainMenu : System<DefaultUIContext> {
  EntityID new_game = -1;

  void for_each_with(Entity &root, DefaultUIContext &ctx, float) override {
    new_game = button(ctx, mk(root, 0), "New game").id();
    button(ctx, mk(root, 1), "Quit");
  }
};

int main() {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 400;
  config.height = 200;
  if (!graphics::init(config))
    return 1;

  input::add_singleton_components(EntityHelper::createEntity(),
                                  ui::default_keymap<DefaultAction>());
  SystemManager systems;
  input::register_update_systems(systems);
  auto menu = std::make_unique<MainMenu>();
  MainMenu &screen = *menu;
  ui::setup_with_resolution<>(systems, {400, 200}, std::move(menu));
  auto &ctx = *EntityHelper::get_singleton_cmp<DefaultUIContext>();
  ctx.grab_initial_focus = false;
  testing::platform_input::set_test_mode(true);

  for (int frame = 0; frame < 3; ++frame)
    systems.run(1.f / 60.f);
  const bool focused_on_load = ctx.focus_id != ctx.ROOT;

  testing::test_input::push_key(keys::TAB);
  for (int frame = 0; frame < 3; ++frame) {
    systems.run(1.f / 60.f);
    testing::test_input::reset_frame();
  }
  const bool tab_focused_first = ctx.has_focus(screen.new_game);

  std::printf("focused on load: %s; after Tab, New game focused: %s\n",
              focused_on_load ? "yes" : "no", tab_focused_first ? "yes" : "no");
  graphics::shutdown();
  return !focused_on_load && tab_focused_first ? 0 : 1;
}
