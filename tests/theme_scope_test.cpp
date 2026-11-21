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

TEST(scoped_themes_survive_to_render_for_plain_labels) {
  ImmTestHarness h;
  h.context().set_theme(green_theme());

  Theme red_font = red_theme();
  red_font.font = Color{200, 0, 0, 255};
  Theme green_font = green_theme();
  green_font.font = Color{0, 200, 0, 255};

  EntityID red_id = -1, green_id = -1;
  {
    ThemeScopeT<ui_test::TestInputAction> scope(h.context(), red_font);
    auto w = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}
                     .with_size(ComponentSize{pixels(80), pixels(20)})
                     .with_label("red").with_auto_text_color(false)
                     .with_debug_name("red_label"));
    red_id = w.ent().id;
  }
  {
    ThemeScopeT<ui_test::TestInputAction> scope(h.context(), green_font);
    auto w = div(h.context(), mk(h.root(), 1),
                 ComponentConfig{}
                     .with_size(ComponentSize{pixels(80), pixels(20)})
                     .with_label("green").with_auto_text_color(false)
                     .with_debug_name("green_label"));
    green_id = w.ent().id;
  }

  const auto &calls = h.render();
  bool saw_red = false, saw_green = false;
  for (const auto &c : calls) {
    if (c.op != "text")
      continue;
    if (c.entity_id == red_id) {
      saw_red = true;
      CHECK(c.color.r > 150 && c.color.g < 60);
    }
    if (c.entity_id == green_id) {
      saw_green = true;
      CHECK(c.color.g > 150 && c.color.r < 60);
    }
  }
  CHECK(saw_red);
  CHECK(saw_green);
}

TEST(direct_theme_mutation_does_not_recolor_earlier_widgets_at_render) {
  ImmTestHarness h;
  h.context().set_theme(green_theme());
  Theme green_font = green_theme();
  green_font.font = Color{0, 200, 0, 255};
  h.context().set_theme(green_font);

  auto before = div(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size(ComponentSize{pixels(80), pixels(20)})
                        .with_label("before").with_auto_text_color(false)
                        .with_debug_name("before_label"));

  h.context().theme.font = Color{200, 0, 0, 255};
  auto after = div(h.context(), mk(h.root(), 1),
                   ComponentConfig{}
                       .with_size(ComponentSize{pixels(80), pixels(20)})
                       .with_label("after").with_auto_text_color(false)
                       .with_debug_name("after_label"));

  const auto &calls = h.render();
  for (const auto &c : calls) {
    if (c.op != "text")
      continue;
    if (c.entity_id == before.ent().id)
      CHECK(c.color.g > 150 && c.color.r < 60);
    if (c.entity_id == after.ent().id)
      CHECK(c.color.r > 150 && c.color.g < 60);
  }
}

TEST(theme_set_one_frame_does_not_leak_into_the_next) {
  ImmTestHarness h;
  Theme app = green_theme();
  app.font = Color{0, 200, 0, 255};
  ThemeDefaults::get().set_theme(app);

  ThemeDefaults::get().begin_frame();
  h.context().theme = ThemeDefaults::get().get_theme();
  h.context().set_theme(red_theme());
  CHECK(h.context().theme.primary.r > 150);

  ThemeDefaults::get().begin_frame();
  h.context().theme = ThemeDefaults::get().get_theme();
  CHECK(h.context().theme.primary.g > 150);
  CHECK(ThemeDefaults::get().theme.primary.g > 150);
}

int main() { return ui_test::run_registered_tests("theme scope"); }
