
#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
bool any_rect_at(const std::vector<DrawCall> &calls, float x, float y) {
  for (const auto &c : calls)
    if (c.op == "rectangle" && c.rect.x == x && c.rect.y == y)
      return true;
  return false;
}
}

TEST(world_draw_before_ui_is_cleared_without_a_frame_boundary) {
  ImmTestHarness h;
  capture::clear();
  capture::enable();
  capture::record("rectangle", RectangleType{5, 5, 20, 20},
                  ColorType{255, 0, 0, 255});
  h.begin_frame();
  auto r = button(h.context(), mk(h.root(), 0),
                  ComponentConfig{}.with_label("hi").with_debug_name("btn"));
  (void)r;
  const auto &calls = h.render();

  CHECK(!calls.empty());
  CHECK(!any_rect_at(calls, 5, 5));
}

TEST(world_draws_survive_the_imm_render_pass) {
  ImmTestHarness h;
  capture::clear();
  capture::begin_frame();
  capture::enable();
  capture::record("rectangle", RectangleType{5, 5, 20, 20},
                  ColorType{255, 0, 0, 255});
  h.begin_frame();
  auto r = button(h.context(), mk(h.root(), 0),
                  ComponentConfig{}.with_label("hi").with_debug_name("btn"));
  const EntityID id = r.ent().id;
  const auto &calls = h.render();

  CHECK(!calls.empty());
  CHECK(any_rect_at(calls, 5, 5));
  bool saw_widget = false;
  for (const auto &c : calls)
    if (c.entity_id == id)
      saw_widget = true;
  CHECK(saw_widget);
}

TEST(world_draws_survive_the_batched_render_pass) {
  ImmTestHarness h;
  capture::clear();
  capture::begin_frame();
  capture::enable();
  capture::record("rectangle", RectangleType{7, 7, 20, 20},
                  ColorType{0, 255, 0, 255});
  h.begin_frame();
  auto r = button(h.context(), mk(h.root(), 0),
                  ComponentConfig{}.with_label("hi").with_debug_name("btn"));
  (void)r;
  const auto &calls = h.render_batched();

  CHECK(!calls.empty());
  CHECK(any_rect_at(calls, 7, 7));
}

int main() { return ui_test::run_registered_tests("capture frame boundary tests"); }
