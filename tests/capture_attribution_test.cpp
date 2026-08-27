// A recorded draw says WHICH widget made it.
//
// Without that, capture answers "something drew a blue outline" when the
// question is "did this row's border get painted". Both renderers are
// selectable at runtime, so both have to attribute, or an assertion passes on
// one path and not the other.

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
// Every draw carrying an id that belongs to the tree, not the -1 that means
// "nobody claimed this".
bool all_attributed(const std::vector<DrawCall> &calls) {
  if (calls.empty())
    return false;
  for (const auto &c : calls)
    if (c.entity_id < 0)
      return false;
  return true;
}

bool any_from(const std::vector<DrawCall> &calls, EntityID id) {
  for (const auto &c : calls)
    if (c.entity_id == id)
      return true;
  return false;
}
} // namespace

TEST(immediate_renderer_attributes_draws_to_the_widget) {
  ImmTestHarness h;
  capture::clear();
  h.begin_frame();
  auto r = button(h.context(), mk(h.root(), 0),
                  ComponentConfig{}.with_label("hi").with_debug_name("btn"));
  const EntityID id = r.ent().id;
  const auto &calls = h.render();

  CHECK(!calls.empty());
  CHECK(any_from(calls, id));
  CHECK(all_attributed(calls));
}

TEST(batched_renderer_attributes_draws_to_the_widget) {
  ImmTestHarness h;
  capture::clear();
  h.begin_frame();
  auto r = button(h.context(), mk(h.root(), 0),
                  ComponentConfig{}.with_label("hi").with_debug_name("btn"));
  const EntityID id = r.ent().id;
  const auto &calls = h.render_batched();

  CHECK(!calls.empty());
  CHECK(any_from(calls, id));
  CHECK(all_attributed(calls));
}

// Two widgets, two ids: attribution has to distinguish them, not stamp the
// whole frame with whoever rendered last.
TEST(sibling_widgets_get_their_own_ids) {
  ImmTestHarness h;
  capture::clear();
  h.begin_frame();
  auto a = button(h.context(), mk(h.root(), 0),
                  ComponentConfig{}.with_label("a").with_debug_name("a"));
  auto b = button(h.context(), mk(h.root(), 1),
                  ComponentConfig{}.with_label("b").with_debug_name("b"));
  const EntityID id_a = a.ent().id;
  const EntityID id_b = b.ent().id;
  const auto &calls = h.render();

  CHECK(id_a != id_b);
  CHECK(any_from(calls, id_a));
  CHECK(any_from(calls, id_b));
}

// Draws made outside any widget stay honest rather than being blamed on the
// last widget that happened to render.
TEST(a_draw_outside_a_widget_is_unattributed) {
  capture::clear();
  capture::enable();
  { capture::Scope attribute(42, 3); }
  capture::record("rectangle", RectangleType{0, 0, 1, 1}, ColorType{1, 2, 3, 4});
  capture::disable();

  CHECK(capture::calls().size() == size_t{1});
  CHECK(capture::calls()[0].entity_id == -1);
  capture::clear();
}

int main() { return ui_test::run_registered_tests("capture attribution tests"); }
