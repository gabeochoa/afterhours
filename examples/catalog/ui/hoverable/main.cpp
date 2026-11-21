#include <cstdio>

#include "../../../../ah.h"
#include "../../../../src/graphics.h"
#define AFTER_HOURS_IMM_UI
#include "../../../../src/plugins/e2e_testing/platform_test_input.h"
#include "../../../../src/plugins/ui.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

struct Transcript : System<DefaultUIContext> {
  bool copy_shown[2] = {};

  void for_each_with(Entity &root, DefaultUIContext &ctx, float) override {
    for (int i = 0; i < 2; ++i) {
      auto turn = div(ctx, mk(root, i),
                      ComponentConfig{}
                          .with_size({pixels(360), pixels(48)})
                          .with_absolute_position(0.f, 60.f * static_cast<float>(i))
                          .with_custom_background(colors::UI_BLACK)
                          .with_hoverable()
                          .with_skip_hover_override());
      copy_shown[i] = ctx.mouse_was_in_subtree(turn.id());
      if (copy_shown[i])
        button(ctx, mk(turn.ent(), 0), "Copy");
    }
  }
};

int main() {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 400;
  config.height = 200;
  if (!graphics::init(config))
    return 1;

  SystemManager systems;
  auto transcript = std::make_unique<Transcript>();
  Transcript &screen = *transcript;
  ui::setup_with_resolution<>(systems, {400, 200}, std::move(transcript));

  testing::platform_input::set_test_mode(true);
  testing::test_input::set_mouse_position(100.f, 80.f);
  for (int frame = 0; frame < 3; ++frame)
    systems.run(1.f / 60.f);

  std::printf("pointer over the second turn: copy on first %s, second %s\n",
              screen.copy_shown[0] ? "shown" : "hidden",
              screen.copy_shown[1] ? "shown" : "hidden");
  graphics::shutdown();
  return !screen.copy_shown[0] && screen.copy_shown[1] ? 0 : 1;
}
