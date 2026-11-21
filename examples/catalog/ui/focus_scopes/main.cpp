#include <cstdio>

#include "../../../../ah.h"
#include "../../../../src/graphics.h"
#define AFTER_HOURS_IMM_UI
#include "../../../../src/plugins/e2e_testing/platform_test_input.h"
#include "../../../../src/plugins/ui.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

// Two panes, each a focus scope. Focus starts in the left pane and Tab
// cycles inside it, wrapping at its edges; the right pane is reached by
// clicking, not by tabbing past the end.
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
  auto panes = std::make_unique<TwoPanes>();
  TwoPanes &screen = *panes;
  ui::setup_with_resolution<>(systems, {400, 200}, std::move(panes));
  auto &ctx = *EntityHelper::get_singleton_cmp<DefaultUIContext>();
  testing::platform_input::set_test_mode(true);

  for (int frame = 0; frame < 3; ++frame)
    systems.run(1.f / 60.f);

  auto tab = [&] {
    testing::test_input::push_key(keys::TAB);
    for (int frame = 0; frame < 3; ++frame) {
      systems.run(1.f / 60.f);
      testing::test_input::reset_frame();
    }
  };

  const bool starts_left = ctx.has_focus(screen.left_first);
  tab();
  const bool to_second = ctx.has_focus(screen.left_second);
  tab(); // wraps inside the left pane instead of reaching Right 1
  const bool wrapped = ctx.has_focus(screen.left_first);
  const bool never_right = !ctx.has_focus(screen.right_first);

  std::printf("starts on Left 1: %s; Tab -> Left 2: %s; Tab wraps to "
              "Left 1: %s; Right 1 never focused: %s\n",
              starts_left ? "yes" : "no", to_second ? "yes" : "no",
              wrapped ? "yes" : "no", never_right ? "yes" : "no");
  graphics::shutdown();
  return starts_left && to_second && wrapped && never_right ? 0 : 1;
}
