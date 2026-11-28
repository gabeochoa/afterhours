#include <cstdio>

#define AFTER_HOURS_EXAMPLE_UI
#define AFTER_HOURS_EXAMPLE_GRAPHICS
#define AFTER_HOURS_EXAMPLE_TEST_INPUT
#include "../../../example_helper.h"

using namespace afterhours;
using namespace afterhours::examples;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

struct TwoPanes : System<DefaultUIContext> {
  EntityID left_first = -1;
  EntityID left_second = -1;
  EntityID right_first = -1;

  void for_each_with(Entity &root, DefaultUIContext &ctx, float) override {
    auto left = div(ctx, mk(root, 0),
                    ComponentConfig{}.with_focus_scope());
    left_first = button(ctx, mk(left.ent(), 0), "Left 1").id();
    left_second = button(ctx, mk(left.ent(), 1), "Left 2").id();
    auto right = div(ctx, mk(root, 1),
                     ComponentConfig{}.with_focus_scope());
    right_first = button(ctx, mk(right.ent(), 0), "Right 1").id();
    button(ctx, mk(right.ent(), 1), "Right 2");
  }
};

int main() {
  Checks checks;
  input::add_singleton_components(EntityHelper::createEntity(),
                                  ui::default_keymap<DefaultAction>());
  HeadlessUI<TwoPanes> app(400, 200, std::make_unique<TwoPanes>(),
                           std::make_unique<input::InputSystem>());
  if (!app.ok()) return 1;
  auto &ctx = app.ui.context();
  auto &screen = app.screen();

  app.run(3);

  auto tab = [&] {
    testing::test_input::push_key(keys::TAB);
    for (int frame = 0; frame < 3; ++frame) {
      app.run();
      testing::test_input::reset_frame();
    }
  };

  const bool starts_left = ctx.has_focus(screen.left_first);
  tab();
  const bool to_second = ctx.has_focus(screen.left_second);
  tab();
  const bool wrapped = ctx.has_focus(screen.left_first);
  const bool never_right = !ctx.has_focus(screen.right_first);

  std::printf("starts on Left 1: %s; Tab -> Left 2: %s; Tab wraps to "
              "Left 1: %s; Right 1 never focused: %s\n",
              starts_left ? "yes" : "no", to_second ? "yes" : "no",
              wrapped ? "yes" : "no", never_right ? "yes" : "no");
  checks.check(starts_left, "starts on Left 1");
  checks.check(to_second, "Tab moves to Left 2");
  checks.check(wrapped, "Tab wraps to Left 1");
  checks.check(never_right, "Right pane never focused");
  return checks.finish("focus_scopes");
}
