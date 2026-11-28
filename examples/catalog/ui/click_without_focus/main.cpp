#include <cstdio>

#define AFTER_HOURS_EXAMPLE_UI
#define AFTER_HOURS_EXAMPLE_GRAPHICS
#define AFTER_HOURS_EXAMPLE_TEST_INPUT
#include "../../../example_helper.h"

using namespace afterhours;
using namespace afterhours::examples;
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

int main() {
  Checks checks;
  HeadlessUI<Chat> app(300, 100, std::make_unique<Chat>());
  if (!app.ok()) return 1;

  app.run();
  app.click_at(100, 45);
  const bool reply_focused = app.ui.context().has_focus(app.screen().composer);
  app.click_at(100, 15);

  std::printf("clicked Reply (focused: %s), then Show summary: summary %s, "
              "Reply still focused: %s\n",
              reply_focused ? "yes" : "no",
              app.screen().summary_open ? "open" : "closed",
              app.ui.context().has_focus(app.screen().composer) ? "yes" : "no");
  checks.check(reply_focused, "Reply takes focus when clicked");
  checks.check(app.screen().summary_open, "summary button still activates");
  checks.check(app.ui.context().has_focus(app.screen().composer),
               "summary button does not steal focus");
  return checks.finish("click_without_focus");
}
