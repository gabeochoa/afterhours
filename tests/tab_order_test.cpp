// Tab follows tree order, not allocation order (which made floatinghotel's
// file tree need 7 Tabs to reach its first row); FocusScope traps Tab (hanabi #336).

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using ui_test::ImmTestHarness;

namespace {

void press_tab(UIContext<ui_test::TestInputAction> &ctx) {
  ctx.last_action = ui_test::TestInputAction::WidgetNext;
  ctx.process_tabbing(ctx.focus_id);
}

void press_shift_tab(UIContext<ui_test::TestInputAction> &ctx) {
  ctx.last_action = ui_test::TestInputAction::WidgetBack;
  ctx.process_tabbing(ctx.focus_id);
}

struct Panes {
  EntityID a1, a2, b1;
};

// One builder for both emission orders so entities are identical; only tree order changes.
Panes build_panes(ImmTestHarness &h, bool a_first, bool scope_a = false) {
  using namespace imm;
  auto &ctx = h.context();
  Panes out{-1, -1, -1};
  const int order[2] = {a_first ? 0 : 1, a_first ? 1 : 0};
  for (int pi : order) {
    ComponentConfig pane_cfg;
    if (scope_a && pi == 0)
      pane_cfg.with_focus_scope();
    auto pane = div(ctx, mk(h.root(), pi), pane_cfg);
    if (pi == 0) {
      out.a1 = button(ctx, mk(pane.ent(), 0), ComponentConfig{}).ent().id;
      out.a2 = button(ctx, mk(pane.ent(), 1), ComponentConfig{}).ent().id;
    } else {
      out.b1 = button(ctx, mk(pane.ent(), 0), ComponentConfig{}).ent().id;
    }
  }
  h.layout_only();
  return out;
}

} // namespace

TEST(tree_order_beats_allocation_order) {
  ImmTestHarness h;
  auto &ctx = h.context();
  const Panes alloc = build_panes(h, /*a_first=*/false);
  h.begin_frame();
  const Panes p = build_panes(h, /*a_first=*/true);
  CHECK(p.a1 == alloc.a1 && p.b1 == alloc.b1);

  const std::vector<EntityID> expected{p.a1, p.a2, p.b1};
  CHECK(ctx.build_tab_order() == expected);

  ctx.set_focus(p.a1);
  press_tab(ctx);
  CHECK(ctx.has_focus(p.a2));
  press_tab(ctx);
  CHECK(ctx.has_focus(p.b1));
  press_tab(ctx);
  CHECK(ctx.has_focus(p.a1));
  press_shift_tab(ctx);
  CHECK(ctx.has_focus(p.b1));
}

TEST(flat_list_still_tabs_in_build_order_and_wraps) {
  using namespace imm;
  ImmTestHarness h;
  auto &ctx = h.context();
  h.begin_frame();
  auto a = button(ctx, mk(h.root(), 0), ComponentConfig{});
  auto b = button(ctx, mk(h.root(), 1), ComponentConfig{});
  auto c = button(ctx, mk(h.root(), 2), ComponentConfig{});
  h.layout_only();

  ctx.set_focus(a.ent().id);
  press_tab(ctx);
  CHECK(ctx.has_focus(b.ent().id));
  press_tab(ctx);
  CHECK(ctx.has_focus(c.ent().id));
  press_tab(ctx);
  CHECK(ctx.has_focus(a.ent().id));
}

TEST(focus_scope_traps_tab_and_wraps_at_its_edges) {
  ImmTestHarness h;
  auto &ctx = h.context();
  h.begin_frame();
  const Panes p = build_panes(h, /*a_first=*/true, /*scope_a=*/true);

  CHECK(ctx.nearest_focus_scope(p.a1) != ctx.ROOT);
  CHECK(ctx.nearest_focus_scope(p.b1) == ctx.ROOT);

  ctx.set_focus(p.a1);
  press_tab(ctx);
  CHECK(ctx.has_focus(p.a2));
  press_tab(ctx);
  CHECK(ctx.has_focus(p.a1));
  press_shift_tab(ctx);
  CHECK(ctx.has_focus(p.a2));
}

// Unscoped Tab is how focus enters a scope.
TEST(unscoped_focus_tabs_the_whole_list) {
  using namespace imm;
  ImmTestHarness h;
  auto &ctx = h.context();
  h.begin_frame();
  auto outside = button(ctx, mk(h.root(), 2), ComponentConfig{});
  const Panes p = build_panes(h, /*a_first=*/true, /*scope_a=*/true);

  const auto order = ctx.build_tab_order();
  CHECK(order.size() == 4);
  CHECK(order.front() == outside.ent().id);
  ctx.set_focus(outside.ent().id);
  press_tab(ctx);
  CHECK(ctx.has_focus(order[1]));
  CHECK(ctx.has_focus(p.a1));
}

TEST(nested_scope_nearest_one_wins) {
  using namespace imm;
  ImmTestHarness h;
  auto &ctx = h.context();
  h.begin_frame();
  auto outer = div(ctx, mk(h.root(), 0),
                   ComponentConfig{}.with_focus_scope());
  auto o1 = button(ctx, mk(outer.ent(), 0), ComponentConfig{});
  auto inner = div(ctx, mk(outer.ent(), 1),
                   ComponentConfig{}.with_focus_scope());
  auto i1 = button(ctx, mk(inner.ent(), 0), ComponentConfig{});
  auto i2 = button(ctx, mk(inner.ent(), 1), ComponentConfig{});
  auto o2 = button(ctx, mk(outer.ent(), 2), ComponentConfig{});
  h.layout_only();

  ctx.set_focus(i1.ent().id);
  press_tab(ctx);
  CHECK(ctx.has_focus(i2.ent().id));
  press_tab(ctx);
  CHECK(ctx.has_focus(i1.ent().id));

  ctx.set_focus(o1.ent().id);
  press_tab(ctx);
  CHECK(ctx.has_focus(o2.ent().id));
  press_tab(ctx);
  CHECK(ctx.has_focus(o1.ent().id));
}

TEST(widget_mod_reverses_tab) {
  using namespace imm;
  ImmTestHarness h;
  auto &ctx = h.context();
  h.begin_frame();
  auto a = button(ctx, mk(h.root(), 0), ComponentConfig{});
  auto b = button(ctx, mk(h.root(), 1), ComponentConfig{});
  auto c = button(ctx, mk(h.root(), 2), ComponentConfig{});
  h.layout_only();

  ctx.set_focus(b.ent().id);
  const auto mod_idx =
      magic_enum::enum_index<ui_test::TestInputAction>(
          ui_test::TestInputAction::WidgetMod)
          .value();
  ctx.all_actions[mod_idx] = true;
  press_tab(ctx);
  CHECK(ctx.has_focus(a.ent().id));
  (void)c;
}

TEST(handle_tabbing_publishes_tree_order_and_grabs_its_first) {
  ImmTestHarness h;
  auto &ctx = h.context();
  EntityHelper::registerSingleton<UIContext<ui_test::TestInputAction>>(
      h.context_entity());
  const Panes alloc = build_panes(h, /*a_first=*/false);
  h.begin_frame();
  const Panes p = build_panes(h, /*a_first=*/true);
  CHECK(p.a1 == alloc.a1);

  HandleTabbing<ui_test::TestInputAction> tabbing;
  tabbing.once(0.f);
  CHECK(tabbing.context == &ctx);
  CHECK(ctx.focus_order == std::vector<EntityID>({p.a1, p.a2, p.b1}));
  CHECK(ctx.has_focus(p.a1));
}

int main() { return ui_test::run_registered_tests("tab order tests"); }
