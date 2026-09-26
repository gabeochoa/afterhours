// small_components_test.cpp
// The imm helpers consumers kept hand-rolling: a static divider line, a
// value pill, and a compact settings list with label/value rows.

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

TEST(divider_line_spans_the_container_at_one_pixel) {
  ImmTestHarness h;
  h.begin_frame();
  auto col = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}.with_size({pixels(400), pixels(200)}));
  divider_line(h.context(), mk(col.ent(), 0), Axis::X,
               ComponentConfig{}.with_debug_name("rule_h"));
  auto row = div(h.context(), mk(h.root(), 1),
                 ComponentConfig{}
                     .with_size({pixels(400), pixels(200)})
                     .with_flex_direction(FlexDirection::Row));
  divider_line(h.context(), mk(row.ent(), 0), Axis::Y,
               ComponentConfig{}.with_debug_name("rule_v"));
  h.layout_only();

  auto *h_rule = h.find("rule_h");
  CHECK(h_rule != nullptr);
  if (h_rule) {
    CHECK_APPROX(h_rule->rect().width, 400.f);
    CHECK_APPROX(h_rule->rect().height, 1.f);
  }
  auto *v_rule = h.find("rule_v");
  CHECK(v_rule != nullptr);
  if (v_rule) {
    CHECK_APPROX(v_rule->rect().width, 1.f);
    CHECK_APPROX(v_rule->rect().height, 200.f);
  }
}

TEST(value_pill_sizes_to_its_text_with_pill_height) {
  ImmTestHarness h;
  h.begin_frame();
  auto short_pill = value_pill(h.context(), mk(h.root(), 0), "7",
                               ComponentConfig{}.with_debug_name("pill_s"));
  auto long_pill = value_pill(h.context(), mk(h.root(), 1), "128 items/min",
                              ComponentConfig{}.with_debug_name("pill_l"));
  h.layout_only();

  CHECK(short_pill.ent().has<HasLabel>());
  if (short_pill.ent().has<HasLabel>())
    CHECK(short_pill.ent().get<HasLabel>().label == "7");
  auto *s = h.find("pill_s");
  auto *l = h.find("pill_l");
  CHECK(s != nullptr && l != nullptr);
  if (s && l) {
    CHECK_APPROX(s->rect().height, 28.f);
    CHECK_APPROX(l->rect().height, 28.f);
    CHECK(l->rect().width > s->rect().width);
    // Text plus the 12px horizontal padding on each side.
    CHECK(s->rect().width >= 25.f);
  }
}

TEST(value_pill_centers_its_text_unless_the_caller_says_otherwise) {
  ImmTestHarness h;
  h.begin_frame();
  auto centered = value_pill(h.context(), mk(h.root(), 0), "42",
                             ComponentConfig{}.with_debug_name("pill_c"));
  auto left = value_pill(h.context(), mk(h.root(), 1), "42",
                         ComponentConfig{}
                             .with_alignment(TextAlignment::Left)
                             .with_debug_name("pill_l"));
  CHECK(centered.ent().has<HasLabel>());
  if (centered.ent().has<HasLabel>())
    CHECK(centered.ent().get<HasLabel>().alignment == TextAlignment::Center);
  CHECK(left.ent().has<HasLabel>());
  if (left.ent().has<HasLabel>())
    CHECK(left.ent().get<HasLabel>().alignment == TextAlignment::Left);
}

TEST(settings_rows_align_values_right_and_stack_with_dividers) {
  ImmTestHarness h;
  h.begin_frame();
  auto list = settings_list(h.context(), mk(h.root(), 0),
                            ComponentConfig{}
                                .with_size({pixels(400), children()})
                                .with_debug_name("settings"));
  auto value_a = settings_row(h.context(), mk(list.ent(), 0), "Volume",
                              ComponentConfig{}.with_debug_name("row_a"));
  value_pill(h.context(), mk(value_a.ent(), 0), "80%",
             ComponentConfig{}.with_debug_name("value_a"));
  auto value_b = settings_row(h.context(), mk(list.ent(), 1), "Brightness",
                              ComponentConfig{}.with_debug_name("row_b"));
  value_pill(h.context(), mk(value_b.ent(), 0), "40%",
             ComponentConfig{}.with_debug_name("value_b"));
  h.layout_only();

  auto *row_a = h.find("row_a");
  auto *row_b = h.find("row_b");
  auto *pill_a = h.find("value_a");
  auto *pill_b = h.find("value_b");
  CHECK(row_a != nullptr && row_b != nullptr);
  CHECK(pill_a != nullptr && pill_b != nullptr);
  if (row_a && row_b) {
    // 36px line + 1px divider per row.
    CHECK_APPROX(row_b->rect().y - row_a->rect().y, 37.f);
    CHECK_APPROX(row_a->rect().width, 400.f);
  }
  if (row_a && pill_a) {
    // The value sits at the row's right edge, not after the label.
    CHECK_APPROX(pill_a->rect().x + pill_a->rect().width,
                 row_a->rect().x + row_a->rect().width);
  }
  if (pill_a && pill_b)
    CHECK_APPROX(pill_a->rect().x + pill_a->rect().width,
                 pill_b->rect().x + pill_b->rect().width);
}

int main() { return ui_test::run_registered_tests("small components"); }
