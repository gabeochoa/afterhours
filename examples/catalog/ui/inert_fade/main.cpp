#include <algorithm>
#include <cstdio>

#include "../../../../ah.h"
#include "../../../../src/graphics.h"
#define AFTER_HOURS_IMM_UI
#include "../../../../src/plugins/e2e_testing/platform_test_input.h"
#include "../../../../src/plugins/ui.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

struct PauseMenu : System<DefaultUIContext> {
  float opacity = 1.f;
  bool closing = false;
  int quits = 0;

  void for_each_with(Entity &root, DefaultUIContext &ctx, float dt) override {
    if (closing)
      opacity = std::max(0.f, opacity - dt * 4.f);
    auto panel = div(ctx, mk(root, 0),
                     ComponentConfig{}
                         .with_size({pixels(200), pixels(80)})
                         .with_opacity(opacity)
                         .with_inert(closing));
    if (button(ctx, mk(panel.ent(), 0),
               ComponentConfig{}.with_size({pixels(200), pixels(40)}).with_label("Quit")))
      ++quits;
  }
};

static void click_quit(SystemManager &systems) {
  testing::test_input::simulate_click(100.f, 20.f);
  for (int frame = 0; frame < 3; ++frame) {
    systems.run(1.f / 60.f);
    testing::test_input::reset_frame();
  }
}

int main() {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 300;
  config.height = 120;
  if (!graphics::init(config))
    return 1;

  SystemManager systems;
  auto menu = std::make_unique<PauseMenu>();
  PauseMenu &pause = *menu;
  ui::setup_with_resolution<>(systems, {300, 120}, std::move(menu));
  testing::platform_input::set_test_mode(true);
  systems.run(1.f / 60.f);

  click_quit(systems);
  const int while_open = pause.quits;
  pause.closing = true;
  click_quit(systems);
  const int while_fading = pause.quits - while_open;
  std::printf("Quit clicks counted: %d while open, %d while fading (opacity %.2f)\n",
              while_open, while_fading, static_cast<double>(pause.opacity));
  graphics::shutdown();
  return while_open == 1 && while_fading == 0 ? 0 : 1;
}
