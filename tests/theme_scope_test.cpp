// theme_scope_test.cpp
// ThemeScope gives a subtree its own theme and puts the old one back, so
// persistent chrome stops changing colour with whatever screen is up.

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
Theme green_theme() {
  Theme t;
  t.primary = Color{0, 200, 0, 255};
  return t;
}
Theme red_theme() {
  Theme t;
  t.primary = Color{200, 0, 0, 255};
  return t;
}
} // namespace

TEST(scope_gives_a_subtree_its_own_theme) {
  ImmTestHarness h;
  h.context().set_theme(green_theme());

  auto outside = div(h.context(), mk(h.root(), 0),
                     ComponentConfig{}
                         .with_size(ComponentSize{pixels(50), pixels(20)})
                         .with_color_usage(Theme::Usage::Primary)
                         .with_debug_name("outside"));

  auto make_inside = [&]() {
    ThemeScopeT<ui_test::TestInputAction> scope(h.context(), red_theme());
    return div(h.context(), mk(h.root(), 1),
               ComponentConfig{}
                   .with_size(ComponentSize{pixels(50), pixels(20)})
                   .with_color_usage(Theme::Usage::Primary)
                   .with_debug_name("inside"));
  };
  auto inside = make_inside();

  h.layout_only();

  CHECK(outside.ent().has<HasColor>());
  CHECK(inside.ent().has<HasColor>());
  if (outside.ent().has<HasColor>() && inside.ent().has<HasColor>()) {
    CHECK(outside.ent().get<HasColor>().color().g > 150); // green theme
    CHECK(inside.ent().get<HasColor>().color().r > 150);  // red theme
  }
}

TEST(scope_restores_both_the_context_and_the_global) {
  ImmTestHarness h;
  h.context().set_theme(green_theme());
  const Color before_ctx = h.context().theme.primary;
  const Color before_global = ThemeDefaults::get().theme.primary;

  {
    ThemeScopeT<ui_test::TestInputAction> scope(h.context(), red_theme());
    CHECK(h.context().theme.primary.r > 150);
    // Layout reads the global too, or it would colour right and size wrong.
    CHECK(ThemeDefaults::get().theme.primary.r > 150);
  }

  CHECK(h.context().theme.primary.g == before_ctx.g);
  CHECK(ThemeDefaults::get().theme.primary.g == before_global.g);
}

int main() { return ui_test::run_registered_tests("theme scope"); }
