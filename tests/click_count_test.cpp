#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

TEST(note_click_counts_within_time_distance_and_target) {
  MousePointerState m;
  m.pos = {100.f, 100.f};
  CHECK(m.note_click(1.0f, 7) == 1);
  CHECK(m.click_count == 1);
  m.pos = {102.f, 101.f};
  CHECK(m.note_click(1.2f, 7) == 2);
  m.pos = {101.f, 100.f};
  CHECK(m.note_click(1.4f, 7) == 3);
  m.pos = {100.f, 100.f};
  CHECK(m.note_click(1.6f, 7) == 1);
}

TEST(note_click_restarts_outside_time_distance_or_target) {
  MousePointerState m;
  m.pos = {100.f, 100.f};
  CHECK(m.note_click(1.0f, 7) == 1);
  CHECK(m.note_click(1.5f, 7) == 1);
  CHECK(m.note_click(1.6f, 7) == 2);
  m.pos = {120.f, 100.f};
  CHECK(m.note_click(1.7f, 7) == 1);
  CHECK(m.note_click(1.8f, 7) == 2);
  m.pos = {121.f, 100.f};
  CHECK(m.note_click(1.9f, 8) == 1);
}

TEST(reset_click_count_clears_the_sequence) {
  MousePointerState m;
  m.pos = {10.f, 10.f};
  CHECK(m.note_click(1.0f, 3) == 1);
  CHECK(m.note_click(1.1f, 3) == 2);
  m.reset_click_count();
  CHECK(m.click_count == 0);
  CHECK(m.note_click(1.2f, 3) == 1);
}

TEST(double_and_triple_click_answer_for_the_hovered_subtree_on_release) {
  ui_test::ImmTestHarness h;
  h.begin_frame();
  auto row = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}.with_size({pixels(100), pixels(30)}));
  row.ent().addComponentIfMissing<HasClickListener>([](Entity &) {});
  auto cell = div(h.context(), mk(row.ent(), 0),
                  ComponentConfig{}.with_size({pixels(20), pixels(20)}));
  cell.ent().addComponentIfMissing<HasClickListener>([](Entity &) {});
  auto other = div(h.context(), mk(h.root(), 1),
                   ComponentConfig{}.with_size({pixels(100), pixels(30)}));
  other.ent().addComponentIfMissing<HasClickListener>([](Entity &) {});
  h.layout_only();

  h.context().prev_hot_id = cell.ent().id;
  h.context().mouse.click_count = 2;
  CHECK(!h.context().is_double_click(row.ent().id));
  h.context().mouse.just_released = true;
  CHECK(h.context().is_double_click(row.ent().id));
  CHECK(!h.context().is_double_click(other.ent().id));
  CHECK(!h.context().is_triple_click(row.ent().id));
  CHECK(!h.context().is_right_click(row.ent().id));
  h.context().mouse.click_count = 3;
  CHECK(h.context().is_triple_click(row.ent().id));
  CHECK(!h.context().is_double_click(row.ent().id));
  h.context().mouse.click_count = 1;
  CHECK(!h.context().is_double_click(row.ent().id));
  CHECK(!h.context().is_triple_click(row.ent().id));
}

int main() { return ui_test::run_registered_tests("click count"); }
