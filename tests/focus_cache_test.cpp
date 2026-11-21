// The focusable set used to be a std::set cleared and refilled every frame:
// one tree-node malloc per focusable widget per frame, plus the full
// can_be_focused walk (ancestor chain + input gates) for every entity.
// HandleTabbing now snapshots every focusability input; while the snapshot
// still matches, the cached order is replayed and no rebuild happens.
//
// Counted, not timed: rebuild counts and allocations are deterministic.

#include "ui_test_harness.h"

#include <cstdlib>

using namespace afterhours;
using namespace afterhours::ui;
using ui_test::ImmTestHarness;
using ui_test::TestInputAction;

static std::size_t g_allocs = 0;
static bool g_counting = false;
void *operator new(std::size_t n) {
  if (g_counting)
    g_allocs++;
  return std::malloc(n);
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }

namespace {

struct TabRig {
  ImmTestHarness h;
  HandleTabbing<TestInputAction> tabbing;
  std::vector<Entity *> ents;

  TabRig() {
    EntityHelper::registerSingleton<UIContext<TestInputAction>>(
        h.context_entity());
    tabbing.context = &h.context();
  }

  // One frame: rebuild the widgets, lay out, run tabbing, end the frame the
  // way EndUIContextManager does (focused_ids cleared, storage kept).
  void frame(bool skip_middle = false, int count = 3) {
    using namespace afterhours::ui::imm;
    auto &ctx = h.context();
    h.begin_frame();
    ents.clear();
    auto first = button(ctx, mk(h.root(), 0), ComponentConfig{});
    ents.push_back(&first.ent());
    if (count > 1) {
      auto middle = button(ctx, mk(h.root(), 1),
                           ComponentConfig{}.with_skip_tabbing(skip_middle));
      ents.push_back(&middle.ent());
    }
    if (count > 2) {
      auto last = button(ctx, mk(h.root(), 2), ComponentConfig{});
      ents.push_back(&last.ent());
    }
    if (count > 3) {
      auto extra = button(ctx, mk(h.root(), 3), ComponentConfig{});
      ents.push_back(&extra.ent());
    }
    h.layout_only();
    tabbing.once(.016f);
    for (Entity *e : ents)
      tabbing.for_each_with(*e, e->get<UIComponent>(), .016f);
  }

  void end_frame() { h.context().focused_ids.clear(); }

  std::vector<EntityID> expected_membership() {
    std::vector<EntityID> out;
    for (Entity *e : ents)
      if (can_be_focused(h.context(), *e))
        out.push_back(e->id);
    return out;
  }
};

} // namespace

TEST(unchanged_frames_rebuild_once_and_allocate_nothing) {
  TabRig rig;
  rig.frame();
  auto &ctx = rig.h.context();
  CHECK(ctx.focus_rebuild_count == 1);
  const std::vector<EntityID> want = rig.expected_membership();
  CHECK(ctx.focused_ids.ids == want);
  const std::size_t cap = ctx.focused_ids.capacity();
  rig.end_frame();

  std::size_t allocs = 0;
  for (int i = 0; i < 10; i++) {
    rig.frame();
    CHECK(ctx.focus_rebuild_count == 1);
    CHECK(ctx.focused_ids.ids == want);
    CHECK(ctx.focused_ids.capacity() == cap);
    // Measure the tabbing pass itself on the last unchanged frame.
    if (i == 9) {
      ctx.focused_ids.clear();
      g_allocs = 0;
      g_counting = true;
      rig.tabbing.once(.016f);
      for (Entity *e : rig.ents)
        rig.tabbing.for_each_with(*e, e->get<UIComponent>(), .016f);
      g_counting = false;
      allocs = g_allocs;
    }
    rig.end_frame();
  }
  CHECK(allocs == 0);
}

TEST(edits_invalidate_and_membership_matches_the_walk) {
  TabRig rig;
  auto &ctx = rig.h.context();

  rig.frame(false);
  CHECK(ctx.focused_ids.ids == rig.expected_membership());
  const std::size_t after_first = ctx.focus_rebuild_count;
  rig.end_frame();

  // Config skip toggles in both directions.
  rig.frame(true);
  CHECK(ctx.focus_rebuild_count == after_first + 1);
  CHECK(ctx.focused_ids.ids == rig.expected_membership());
  CHECK(!ctx.focused_ids.contains(rig.ents[1]->id));
  rig.end_frame();
  rig.frame(true);
  CHECK(ctx.focus_rebuild_count == after_first + 1);
  rig.end_frame();
  rig.frame(false);
  CHECK(ctx.focus_rebuild_count == after_first + 2);
  CHECK(ctx.focused_ids.ids == rig.expected_membership());
  rig.end_frame();

  // A component added mid-frame, after the widgets were built.
  rig.frame(false);
  rig.end_frame();
  const std::size_t before_manual = ctx.focus_rebuild_count;
  rig.ents[1]->addComponentIfMissing<SkipWhenTabbing>();
  rig.tabbing.once(.016f);
  for (Entity *e : rig.ents)
    rig.tabbing.for_each_with(*e, e->get<UIComponent>(), .016f);
  CHECK(ctx.focus_rebuild_count == before_manual + 1);
  CHECK(!ctx.focused_ids.contains(rig.ents[1]->id));
  rig.end_frame();
  rig.ents[1]->removeComponent<SkipWhenTabbing>();
  rig.tabbing.once(.016f);
  for (Entity *e : rig.ents)
    rig.tabbing.for_each_with(*e, e->get<UIComponent>(), .016f);
  CHECK(ctx.focused_ids.contains(rig.ents[1]->id));
  rig.end_frame();

  // An input gate blocks everything, then goes away.
  ctx.add_input_gate("block", [](EntityID) { return false; });
  rig.tabbing.once(.016f);
  for (Entity *e : rig.ents)
    rig.tabbing.for_each_with(*e, e->get<UIComponent>(), .016f);
  CHECK(ctx.focused_ids.empty());
  rig.end_frame();
  ctx.remove_input_gate("block");
  rig.tabbing.once(.016f);
  for (Entity *e : rig.ents)
    rig.tabbing.for_each_with(*e, e->get<UIComponent>(), .016f);
  CHECK(ctx.focused_ids.ids == rig.expected_membership());
  rig.end_frame();

  // A widget that appears later joins the set; nothing else changes.
  rig.frame(false, 4);
  CHECK(ctx.focused_ids.ids == rig.expected_membership());
  CHECK(ctx.focused_ids.size() == 4);
  rig.end_frame();
}

TEST(tab_navigation_is_identical_on_cached_frames) {
  TabRig rig;
  auto &ctx = rig.h.context();
  rig.frame();
  rig.end_frame();
  CHECK(ctx.focus_rebuild_count == 1);

  ctx.set_focus(rig.ents[0]->id);
  ctx.last_action = TestInputAction::WidgetNext;
  rig.frame();
  CHECK(ctx.focus_rebuild_count == 1);
  CHECK(ctx.has_focus(rig.ents[1]->id));
  rig.end_frame();

  ctx.last_action = TestInputAction::WidgetBack;
  rig.frame();
  CHECK(ctx.focus_rebuild_count == 1);
  CHECK(ctx.has_focus(rig.ents[0]->id));
  ctx.last_action = TestInputAction::None;
  rig.end_frame();
}

int main() { return ui_test::run_registered_tests("focus_cache_test"); }
