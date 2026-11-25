
#include "ui_test_harness.h"

#include <cmath>
#include <cstdio>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
void mark_rendered() {
  for (const auto &e : UICollectionHolder::get().collection.get_entities())
    if (e && e->has<UIComponent>())
      e->get<UIComponent>().was_rendered_to_screen = true;
}

void press_at(UIContext<ui_test::TestInputAction> &ctx, float x, float y) {
  ctx.mouse.pos = input::MousePosition{x, y};
  ctx.mouse.left_down = true;
  ctx.mouse.just_pressed = true;
  ctx.mouse.press_moved = false;
  ctx.set_hot(ctx.ROOT);
  ctx.set_active(ctx.ROOT);
}

bool click_fires(ImmTestHarness &h, Entity &ent, float x, float y) {
  press_at(h.context(), x, y);
  ResolveHitTarget<ui_test::TestInputAction> resolver;
  resolver.context = &h.context();
  resolver.resolve();
  HandleClicks<ui_test::TestInputAction> sys;
  sys.context = &h.context();
  sys.for_each_with(ent, ent.get<UIComponent>(), ent.get<HasClickListener>(), 0.f);
  return ent.get<HasClickListener>().down;
}

ElementResult make_button(ImmTestHarness &h, float rotation, bool origin_tl = false) {
  h.begin_frame();
  auto cfg = ComponentConfig{}
                 .with_size(ComponentSize{pixels(200), pixels(40)})
                 .with_absolute_position(100.f, 100.f)
                 .with_label("R")
                 .with_debug_name("rot_btn");
  if (rotation != 0.f) cfg.with_rotation(rotation);
  if (origin_tl) cfg.with_origin(0.f, 0.f);
  return button(h.context(), mk(h.root(), 0), cfg);
}
}

TEST(rotated_contains_geometry) {
  HasUIModifiers m;
  m.rotation = 90.f;
  const RectangleType r{100.f, 100.f, 200.f, 40.f};
  ui_test::check(m.contains_rotated(r, 200.f, 200.f),
                 "point below the AABB but inside the rotated rect hits", __FILE__, __LINE__);
  ui_test::check(!m.contains_rotated(r, 110.f, 110.f),
                 "AABB corner outside the rotated rect misses", __FILE__, __LINE__);
  ui_test::check(m.contains_rotated(r, 200.f, 120.f), "centre hits", __FILE__, __LINE__);
  m.rotation = 0.f;
  ui_test::check(m.contains_rotated(r, 110.f, 110.f) && !m.contains_rotated(r, 200.f, 200.f),
                 "unrotated behaviour unchanged", __FILE__, __LINE__);
}

TEST(rotated_button_hit_follows_rotation) {
  ImmTestHarness h;
  auto b = make_button(h, 90.f);
  Entity &ent = b.ent();
  h.layout_only();
  mark_rendered();
  UIComponent *cmp = h.find("rot_btn");
  ui_test::check(cmp != nullptr, "button exists", __FILE__, __LINE__);
  if (!cmp) return;
  const float cx = cmp->rect().x + cmp->rect().width * .5f;
  const float cy = cmp->rect().y + cmp->rect().height * .5f;
  ui_test::check(ent.has<HasUIModifiers>() && ent.get<HasUIModifiers>().rotation == 90.f,
                 "with_rotation reaches HasUIModifiers", __FILE__, __LINE__);
  ui_test::check(click_fires(h, ent, cx, cy + 80.f),
                 "click inside rotated bounds (outside AABB) hits", __FILE__, __LINE__);
}

TEST(rotated_button_aabb_corner_misses) {
  ImmTestHarness h;
  auto b = make_button(h, 90.f);
  Entity &ent = b.ent();
  h.layout_only();
  mark_rendered();
  UIComponent *cmp = h.find("rot_btn");
  if (!cmp) { ui_test::check(false, "button exists", __FILE__, __LINE__); return; }
  ui_test::check(!click_fires(h, ent, cmp->rect().x + 5.f, cmp->rect().y + 5.f),
                 "click inside AABB but outside rotated bounds misses", __FILE__, __LINE__);
}

TEST(unrotated_button_corner_still_hits) {
  ImmTestHarness h;
  auto b = make_button(h, 0.f);
  Entity &ent = b.ent();
  h.layout_only();
  mark_rendered();
  UIComponent *cmp = h.find("rot_btn");
  if (!cmp) { ui_test::check(false, "button exists", __FILE__, __LINE__); return; }
  ui_test::check(click_fires(h, ent, cmp->rect().x + 5.f, cmp->rect().y + 5.f),
                 "unrotated corner still hits", __FILE__, __LINE__);
}

TEST(focus_ring_paint_carries_rotation_and_pivot) {
  ImmTestHarness h;
  auto b = make_button(h, 90.f, true);
  Entity &ent = b.ent();
  h.layout_only();
  mark_rendered();
  h.context().focus_id = ent.id;
  h.context().visual_focus_id = ent.id;
  h.context().render_cmds.clear();
  h.context().render_cmds.push_back({ent.id, 0});
  auto paint = afterhours::ui::detail::prepare_focus_paint(h.context());
  ui_test::check(paint.has_value(), "focus paint prepared", __FILE__, __LINE__);
  if (!paint) return;
  ui_test::check(paint->rotation == 90.f, "ring carries the widget rotation", __FILE__, __LINE__);
  UIComponent *cmp = h.find("rot_btn");
  ui_test::check(cmp && std::fabs(paint->rotation_pivot.x - cmp->rect().x) < 1.5f &&
                     std::fabs(paint->rotation_pivot.y - cmp->rect().y) < 1.5f,
                 "ring pivots on the configured origin", __FILE__, __LINE__);
  Arena arena(Arena::DEFAULT_CAPACITY);
  RenderCommandBuffer buffer(arena);
  afterhours::ui::detail::collect_focus_paint(buffer, *paint);
  bool saw = false;
  for (const auto &cmd : buffer.commands())
    if (cmd.type == RenderPrimitiveType::RoundedRectangleOutline &&
        cmd.data.outline.rotation == 90.f &&
        std::fabs(cmd.data.outline.rot_center_x - paint->rotation_pivot.x) < 1e-3f)
      saw = true;
  ui_test::check(saw, "batched ring outline carries rotation + pivot", __FILE__, __LINE__);
}

int main() { return ui_test::run_registered_tests("rotation hit test"); }
