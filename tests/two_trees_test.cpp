// two_trees_test.cpp
// R-069 (hanabi #335): two independent view trees in one window.
//
// The report claims everything a view would scope is a process-wide
// singleton, so two trees collide. This suite builds the asked scenario
// literally -- two AutoLayoutRoots side by side in one collection, one
// UIContext -- and interacts with both: layout, render, hit-test/click
// and focus. Widget identity comes from the SAME call site for both
// trees (build_button below), which is the case #338 says is disjoint
// because mk() hashes parent.id; this pins that at the tree level.
//
// What is deliberately NOT asserted: two UIContext singletons. The UI
// systems resolve the context through get_singleton_cmp, so a second
// context entity is ignored, and Tab order across the two trees is one
// order (hanabi #336, a separate item). One window has one keyboard
// focus; these tests pin that focus MOVES between trees correctly.

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {

Entity &make_root(ImmTestHarness &h) {
  Entity &root = h.coll.createEntity();
  root.addComponent<UIComponent>(root.id);
  root.addComponent<AutoLayoutRoot>();
  auto &cmp = root.get<UIComponent>();
  cmp.set_desired_width(pixels(800));
  cmp.set_desired_height(pixels(600));
  return root;
}

void layout_root(ImmTestHarness &h, Entity &root) {
  h.coll.merge_entity_arrays();
  auto &entities = h.coll.get_entities();
  EntityID max_id = 0;
  for (const auto &e : entities)
    if (e)
      max_id = std::max(max_id, e->id);
  std::vector<Entity *> mapping(static_cast<size_t>(max_id) + 1, nullptr);
  for (const auto &e : entities)
    if (e)
      mapping[static_cast<size_t>(e->id)] = e.get();
  AutoLayout::autolayout(root.get<UIComponent>(),
                         window_manager::Resolution{800, 600}, mapping, false,
                         1.0f);
  for (const auto &e : entities)
    if (e && e->has<UIComponent>())
      e->get<UIComponent>().was_rendered_to_screen = true;
}

// One call site for both trees: identity must still be disjoint, because
// mk() hashes the parent id (hanabi #338).
ElementResult build_button(UIContext<ui_test::TestInputAction> &ctx,
                           Entity &root, float x) {
  return div(ctx, mk(root, 0),
             ComponentConfig{}
                 .with_size({pixels(120), pixels(40)})
                 .with_absolute_position(x, 10.f)
                 .with_label("PaneButton"));
}

void press_at(UIContext<ui_test::TestInputAction> &ctx, float x, float y) {
  ctx.mouse.pos = input::MousePosition{x, y};
  ctx.mouse.left_down = true;
  ctx.mouse.just_pressed = true;
  ctx.mouse.just_released = false;
  ctx.mouse.press_moved = false;
  ctx.set_hot(ctx.ROOT);
  ctx.set_active(ctx.ROOT);
}

void release(UIContext<ui_test::TestInputAction> &ctx) {
  ctx.mouse.left_down = false;
  ctx.mouse.just_pressed = false;
  ctx.mouse.just_released = true;
}

// ResolveHitTarget + HandleClicks in the order a real app runs them,
// driven by hand (same pattern as hit_priority_test).
void run_clicks(ImmTestHarness &h) {
  ResolveHitTarget<ui_test::TestInputAction> resolver;
  resolver.context = &h.context();
  resolver.resolve();

  HandleClicks<ui_test::TestInputAction> sys;
  sys.context = &h.context();
  for (const auto &e : UICollectionHolder::get().collection.get_entities()) {
    if (!e || !e->has<UIComponent>() || !e->has<HasClickListener>())
      continue;
    sys.for_each_with(*e, e->get<UIComponent>(), e->get<HasClickListener>(),
                      0.f);
  }
}

struct TwoTrees {
  ImmTestHarness h;
  Entity *root_b = nullptr;
  Entity *a = nullptr;
  Entity *b = nullptr;

  TwoTrees() {
    root_b = &make_root(h);
    h.begin_frame();
    a = &build_button(h.context(), h.root(), 10.f).ent();
    b = &build_button(h.context(), *root_b, 410.f).ent();
    a->addComponentIfMissing<HasClickListener>([](Entity &) {});
    b->addComponentIfMissing<HasClickListener>([](Entity &) {});
    layout_root(h, h.root());
    layout_root(h, *root_b);
  }
};

} // namespace

TEST(two_trees_from_one_call_site_get_disjoint_entities) {
  TwoTrees t;
  CHECK((*t.a).id != (*t.b).id);
  CHECK((*t.a).id >= 0 && (*t.b).id >= 0);
}

TEST(two_trees_lay_out_side_by_side) {
  TwoTrees t;
  const auto ra = (*t.a).get<UIComponent>().rect();
  const auto rb = (*t.b).get<UIComponent>().rect();
  CHECK_APPROX(ra.x, 10.f);
  CHECK_APPROX(rb.x, 410.f);
  CHECK_APPROX(ra.width, 120.f);
  CHECK_APPROX(rb.width, 120.f);
  // No overlap: the trees occupy disjoint halves of the window.
  CHECK(ra.x + ra.width <= rb.x);
}

TEST(two_trees_both_render_in_one_pass) {
  TwoTrees t;
  const auto &calls = t.h.render();
  bool drew_a = false, drew_b = false;
  for (const auto &c : calls) {
    if (c.entity_id == (*t.a).id)
      drew_a = true;
    if (c.entity_id == (*t.b).id)
      drew_b = true;
  }
  CHECK(drew_a);
  CHECK(drew_b);
}

TEST(two_trees_click_each_independently) {
  TwoTrees t;
  const auto ra = (*t.a).get<UIComponent>().rect();
  const auto rb = (*t.b).get<UIComponent>().rect();

  // Default activation is on press: `down` is the press frame's answer
  // and is reset by the next HandleClicks pass, so read it there.
  press_at(t.h.context(), ra.x + ra.width / 2, ra.y + ra.height / 2);
  run_clicks(t.h);
  CHECK((*t.a).get<HasClickListener>().down);
  CHECK(!(*t.b).get<HasClickListener>().down);
  release(t.h.context());
  run_clicks(t.h);

  press_at(t.h.context(), rb.x + rb.width / 2, rb.y + rb.height / 2);
  run_clicks(t.h);
  CHECK(!(*t.a).get<HasClickListener>().down);
  CHECK((*t.b).get<HasClickListener>().down);
}

TEST(two_trees_focus_moves_between_trees) {
  TwoTrees t;
  auto &ctx = t.h.context();

  ctx.set_focus((*t.a).id);
  CHECK(ctx.has_focus((*t.a).id));
  CHECK(ctx.focus_in_subtree(t.h.root().id));
  CHECK(!ctx.focus_in_subtree(t.root_b->id));

  ctx.set_focus((*t.b).id);
  CHECK(ctx.has_focus((*t.b).id));
  CHECK(!ctx.focus_in_subtree(t.h.root().id));
  CHECK(ctx.focus_in_subtree(t.root_b->id));
}

TEST(two_trees_hot_tracks_only_the_hovered_tree) {
  TwoTrees t;
  const auto ra = (*t.a).get<UIComponent>().rect();
  const auto rb = (*t.b).get<UIComponent>().rect();
  auto &ctx = t.h.context();

  press_at(ctx, ra.x + ra.width / 2, ra.y + ra.height / 2);
  run_clicks(t.h);
  CHECK(ctx.is_hot((*t.a).id));
  CHECK(!ctx.is_hot((*t.b).id));
  CHECK(ctx.mouse_in_subtree(t.h.root().id));
  CHECK(!ctx.mouse_in_subtree(t.root_b->id));

  press_at(ctx, rb.x + rb.width / 2, rb.y + rb.height / 2);
  run_clicks(t.h);
  CHECK(!ctx.is_hot((*t.a).id));
  CHECK(ctx.is_hot((*t.b).id));
}

int main() { return ui_test::run_registered_tests("two trees"); }
