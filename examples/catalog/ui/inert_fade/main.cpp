#include <algorithm>
#include <cstdio>

#define AFTER_HOURS_EXAMPLE_UI
#define AFTER_HOURS_EXAMPLE_GRAPHICS
#define AFTER_HOURS_EXAMPLE_TEST_INPUT
#include "../../../example_helper.h"

using namespace afterhours;
using namespace afterhours::examples;
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

int main() {
  Checks checks;
  HeadlessUI<PauseMenu> app(300, 120, std::make_unique<PauseMenu>());
  if (!app.ok()) return 1;
  app.run();

  app.click_at(100.f, 20.f);
  const int while_open = app.screen().quits;
  app.screen().closing = true;
  app.click_at(100.f, 20.f);
  const int while_fading = app.screen().quits - while_open;
  std::printf("Quit clicks counted: %d while open, %d while fading (opacity %.2f)\n",
              while_open, while_fading, static_cast<double>(app.screen().opacity));
  checks.check(while_open == 1, "Quit counts while open");
  checks.check(while_fading == 0, "Quit inert while fading");
  return checks.finish("inert_fade");
}
