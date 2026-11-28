#include <cstdio>

#define AFTER_HOURS_EXAMPLE_UI
#define AFTER_HOURS_EXAMPLE_GRAPHICS
#define AFTER_HOURS_EXAMPLE_TEST_INPUT
#include "../../../example_helper.h"

using namespace afterhours;
using namespace afterhours::examples;
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
  Checks checks;
  input::add_singleton_components(EntityHelper::createEntity(),
                                  ui::default_keymap<DefaultAction>());
  HeadlessUI<MainMenu> app(400, 200, std::make_unique<MainMenu>(),
                           std::make_unique<input::InputSystem>());
  if (!app.ok())
    return 1;
  auto &ctx = app.ui.context();
  ctx.grab_initial_focus = false;

  app.run(3);
  const bool focused_on_load = ctx.focus_id != ctx.ROOT;

  testing::test_input::push_key(keys::TAB);
  for (int frame = 0; frame < 3; ++frame) {
    app.run(1);
    testing::test_input::reset_frame();
  }
  const bool tab_focused_first = ctx.has_focus(app.screen().new_game);

  std::printf("focused on load: %s; after Tab, New game focused: %s\n",
              focused_on_load ? "yes" : "no", tab_focused_first ? "yes" : "no");
  checks.check(!focused_on_load, "nothing is focused on load");
  checks.check(tab_focused_first, "Tab focuses New game first");
  return checks.finish("no_initial_focus");
}
