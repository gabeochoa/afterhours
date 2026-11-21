// Focus-return tokens: capture where focus sits, move focus away, restore it.
// The generation check is the point: an entity id is reused after deletion,
// so restoring a dead control by id could focus some other control instead.
#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using ui_test::ImmTestHarness;

static Entity &make_focusable(ImmTestHarness &h) {
  Entity &e = h.coll.createEntity();
  e.addComponent<UIComponent>(e.id);
  e.addComponent<HasClickListener>([](Entity &) {});
  return e;
}

static void delete_entity(ImmTestHarness &h, Entity &e) {
  e.cleanup = true;
  h.coll.cleanup();
}

TEST(capture_move_restore_lands_on_the_captured_control) {
  ImmTestHarness h;
  Entity &a = make_focusable(h);
  Entity &b = make_focusable(h);
  h.begin_frame();

  h.context().set_focus(a.id);
  FocusReturnToken token = h.context().capture_focus_return();
  CHECK(token.valid());
  CHECK(token.handle.is_valid());

  h.context().set_focus(b.id);
  CHECK(h.context().restore_focus_return(token));
  CHECK(h.context().focus_id == a.id);
}

TEST(restore_after_delete_is_a_safe_no_op) {
  ImmTestHarness h;
  Entity &a = make_focusable(h);
  Entity &b = make_focusable(h);
  h.begin_frame();

  h.context().set_focus(a.id);
  FocusReturnToken token = h.context().capture_focus_return();
  h.context().set_focus(b.id);
  delete_entity(h, a);

  CHECK(!h.context().restore_focus_return(token));
  CHECK(h.context().focus_id == b.id);
}

// The hard case the token exists for: the id comes back on a new entity. A
// raw id would restore onto the wrong control; the generation refuses.
TEST(restore_does_not_follow_a_reused_id) {
  ImmTestHarness h;
  Entity &a = make_focusable(h);
  Entity &b = make_focusable(h);
  h.begin_frame();

  h.context().set_focus(a.id);
  const EntityID dead_id = a.id;
  FocusReturnToken token = h.context().capture_focus_return();
  h.context().set_focus(b.id);
  delete_entity(h, a);

  Entity &c = make_focusable(h);
  CHECK(c.id == dead_id); // guards: the new entity really reuses the id
  h.begin_frame();

  CHECK(!h.context().restore_focus_return(token));
  CHECK(h.context().focus_id == b.id);
}

TEST(restore_refuses_a_control_that_cannot_take_focus) {
  ImmTestHarness h;
  Entity &a = make_focusable(h);
  Entity &b = make_focusable(h);
  b.addComponent<DoesNotTakeFocus>();
  h.begin_frame();

  h.context().set_focus(b.id);
  FocusReturnToken token = h.context().capture_focus_return();
  h.context().set_focus(a.id);

  CHECK(!h.context().restore_focus_return(token));
  CHECK(h.context().focus_id == a.id);
}

TEST(capture_with_nothing_focused_is_an_empty_token) {
  ImmTestHarness h;
  FocusReturnToken token = h.context().capture_focus_return();
  Entity &b = make_focusable(h);
  h.begin_frame();
  h.context().set_focus(b.id);

  CHECK(!token.valid());
  CHECK(!h.context().restore_focus_return(token));
  CHECK(h.context().focus_id == b.id);
}

// A capture taken before merge still generation-checks: the handle is assigned
// at creation, so a control deleted while still in the temp pool does not
// restore onto whatever takes its id.
TEST(capture_before_merge_still_generation_checks) {
  ImmTestHarness h;
  Entity &a = make_focusable(h);

  h.context().set_focus(a.id);
  FocusReturnToken token = h.context().capture_focus_return();
  CHECK(token.valid());
  CHECK(token.handle.is_valid());

  delete_entity(h, a);
  Entity &b = make_focusable(h);
  h.begin_frame();
  h.context().set_focus(b.id);

  CHECK(!h.context().restore_focus_return(token));
  CHECK(h.context().focus_id == b.id);
}

int main() { return ui_test::run_registered_tests("focus_return"); }
