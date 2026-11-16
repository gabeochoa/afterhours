#include <cstdio>

#include "../../../../ah.h"
#include "../../../../src/graphics.h"
#define AFTER_HOURS_IMM_UI
#include "../../../../src/plugins/e2e_testing/platform_test_input.h"
#include "../../../../src/plugins/ui.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

struct Chat : System<DefaultUIContext> {
  bool summary_open = false;
  EntityID composer = -1;

  void for_each_with(Entity &root, DefaultUIContext &ctx, float) override {
    if (button(ctx, mk(root, 0),
               ComponentConfig{}
                   .with_size({pixels(200), pixels(30)})
                   .with_label(summary_open ? "Hide summary" : "Show summary")
                   .with_takes_focus(false)))
      summary_open = !summary_open;
    composer = button(ctx, mk(root, 1),
                      ComponentConfig{}.with_size({pixels(200), pixels(30)}).with_label("Reply"))
                   .id();
  }
};

static void click_at(SystemManager &systems, float x, float y) {
  testing::test_input::simulate_click(x, y);
  for (int frame = 0; frame < 3; ++frame) {
    systems.run(1.f / 60.f);
    testing::test_input::reset_frame();
  }
}

int main() {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 300;
  config.height = 100;
  if (!graphics::init(config))
    return 1;

  SystemManager systems;
  auto chat = std::make_unique<Chat>();
  Chat &screen = *chat;
  ui::setup_with_resolution<>(systems, {300, 100}, std::move(chat));
  testing::platform_input::set_test_mode(true);
  systems.run(1.f / 60.f);

  auto &ctx = *EntityHelper::get_singleton_cmp<DefaultUIContext>();
  click_at(systems, 100, 45);
  const bool reply_focused = ctx.has_focus(screen.composer);
  click_at(systems, 100, 15);

  std::printf("clicked Reply (focused: %s), then Show summary: summary %s, "
              "Reply still focused: %s\n",
              reply_focused ? "yes" : "no", screen.summary_open ? "open" : "closed",
              ctx.has_focus(screen.composer) ? "yes" : "no");
  graphics::shutdown();
  return reply_focused && screen.summary_open && ctx.has_focus(screen.composer)
             ? 0
             : 1;
}
