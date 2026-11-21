// A removed focused row must not send focus to the first focusable.
//
// floatinghotel's virtualized rows leave the build as you scroll; focus used
// to fall to ROOT and the next frame's try_to_grab handed it to whatever was
// focusable first, so the user was teleported to the top of the list. The
// rule now: next focusable after the dead one in last frame's tab order,
// else previous, else ROOT.

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using ui_test::ImmTestHarness;

namespace {
// Build `slots` buttons (skipping `skip`), run HandleTabbing over them in
// build order, then close the frame. Returns the button entities built.
std::vector<Entity *> build_rows(ImmTestHarness &h, int slots, int skip) {
  using namespace afterhours::ui::imm;
  h.begin_frame();
  std::vector<Entity *> rows;
  for (int i = 0; i < slots; i++) {
    if (i == skip)
      continue;
    auto r = button(h.context(), mk(h.root(), i), ComponentConfig{});
    rows.push_back(&r.ent());
  }
  h.layout_only();
  HandleTabbing<ui_test::TestInputAction> tabbing;
  tabbing.context = &h.context();
  for (Entity *e : rows)
    tabbing.for_each_with(*e, e->get<UIComponent>(), .016f);
  return rows;
}

void end_frame(ImmTestHarness &h) {
  EndUIContextManager<ui_test::TestInputAction>{}.for_each_with(
      h.context_entity(), h.context(), .016f);
}
} // namespace

TEST(focused_row_removed_falls_to_next_sibling) {
  ImmTestHarness h;
  auto &ctx = h.context();
  auto rows = build_rows(h, 3, -1);
  ctx.set_focus(rows[1]->id, FocusSource::Explicit);
  end_frame(h);
  CHECK(ctx.has_focus(rows[1]->id));

  // Middle row leaves the list; its entity is never visited by tabbing.
  auto survivors = build_rows(h, 3, 1);
  CHECK(survivors.size() == 2);
  end_frame(h);
  CHECK(ctx.has_focus(survivors[1]->id));

  // And it stays put on the next frame instead of being re-grabbed.
  auto again = build_rows(h, 3, 1);
  end_frame(h);
  CHECK(ctx.has_focus(again[1]->id));
}

TEST(focused_last_row_removed_falls_to_previous) {
  ImmTestHarness h;
  auto &ctx = h.context();
  auto rows = build_rows(h, 3, -1);
  ctx.set_focus(rows[2]->id, FocusSource::Explicit);
  end_frame(h);

  auto survivors = build_rows(h, 3, 2);
  end_frame(h);
  CHECK(ctx.has_focus(survivors[1]->id));
}

TEST(focused_first_row_removed_falls_to_next_not_first) {
  ImmTestHarness h;
  auto &ctx = h.context();
  auto rows = build_rows(h, 3, -1);
  ctx.set_focus(rows[0]->id, FocusSource::Explicit);
  end_frame(h);

  // Slot 0 is gone; the next sibling after it is slot 1.
  auto survivors = build_rows(h, 3, 0);
  end_frame(h);
  CHECK(ctx.has_focus(survivors[0]->id));
}

TEST(focused_only_row_removed_clears_focus) {
  ImmTestHarness h;
  auto &ctx = h.context();
  auto rows = build_rows(h, 1, -1);
  ctx.set_focus(rows[0]->id, FocusSource::Explicit);
  end_frame(h);

  build_rows(h, 1, 0);
  end_frame(h);
  CHECK(ctx.focus_id == ctx.ROOT);
}

TEST(a_live_focus_is_never_moved_by_the_fallback) {
  ImmTestHarness h;
  auto &ctx = h.context();
  auto rows = build_rows(h, 3, -1);
  ctx.set_focus(rows[2]->id, FocusSource::Explicit);
  end_frame(h);
  auto same = build_rows(h, 3, -1);
  end_frame(h);
  CHECK(ctx.has_focus(same[2]->id));
}

int main() { return ui_test::run_registered_tests("focus fallback tests"); }
