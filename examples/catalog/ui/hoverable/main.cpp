#include <cstdio>

#define AFTER_HOURS_EXAMPLE_UI
#define AFTER_HOURS_EXAMPLE_GRAPHICS
#define AFTER_HOURS_EXAMPLE_TEST_INPUT
#include "../../../example_helper.h"

using namespace afterhours;
using namespace afterhours::examples;
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
  Checks checks;
  HeadlessUI<Transcript> app(400, 200, std::make_unique<Transcript>());
  if (!app.ok()) return 1;

  testing::test_input::set_mouse_position(100.f, 80.f);
  app.run(3);

  std::printf("pointer over the second turn: copy on first %s, second %s\n",
              app.screen().copy_shown[0] ? "shown" : "hidden",
              app.screen().copy_shown[1] ? "shown" : "hidden");
  checks.check(!app.screen().copy_shown[0], "first turn not hovered");
  checks.check(app.screen().copy_shown[1], "second turn hovered");
  return checks.finish("hoverable");
}
