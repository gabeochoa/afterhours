// hit_priority_test.cpp
// D3: no hit-test priority or click consumption for overlapping widgets.
// Reported independently by two apps, and it is the only gap on the list that
// caused a shipped bug -- floatinghotel's manual fallback also failed (on
// D10's coordinate-space mismatch), so a checkout click had no row to land on.
//
// Both reported shapes are here. Reproducing them first, because the prose
// diagnosis ("the row's listener always wins") is a description of the symptom
// and these reports have named the wrong cause before.
//
// What HandleClicks does today, per entity, in iteration order:
//     context->active_if_mouse_inside(entity.id, rect);   // sets hot/active
//     if (context->mouse_activates(entity.id)) { fire(); }
// Resolution is inline, so it depends entirely on iteration order. Worse, the
// two pieces of state it depends on disagree with each other:
//   - set_hot   is LAST-writer-wins (plain assignment)
//   - set_active is FIRST-writer-wins (guarded on is_active(ROOT))
// and is_mouse_press requires BOTH. So an element only fires if it is both the
// first to claim active and the last to set hot.

#include "ui_test_harness.h"

#include <cstdio>
#include <string>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {

// SetVisibity normally sets this during the render pass; these tests drive
// HandleClicks directly, so mark the tree by hand.
void mark_rendered() {
  for (const auto &e : UICollectionHolder::get().collection.get_entities()) {
    if (e && e->has<UIComponent>())
      e->get<UIComponent>().was_rendered_to_screen = true;
  }
}

// A press at `pos`: mouse down, just_pressed, not dragged.
void press_at(UIContext<ui_test::TestInputAction> &ctx, float x, float y) {
  ctx.mouse.pos = input::MousePosition{x, y};
  ctx.mouse.left_down = true;
  ctx.mouse.just_pressed = true;
  ctx.mouse.press_moved = false;
  ctx.set_hot(ctx.ROOT);
  ctx.set_active(ctx.ROOT);
}

// The two systems in the order the real app runs them: resolve which element
// is on top, then let the widgets act on that.
//
// Contexts are assigned rather than resolved via once(): once() looks the
// context up as a singleton, and one cannot be registered per harness -- the
// registry refuses to overwrite, so the second test would read a freed entity.
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

} // namespace

// Shape 1 (floatinghotel F13b): a control inside a clickable row.
//
// The button's fired-ness is read off HasClickListener::down, which
// HandleClicks sets on whichever listener it invokes. An earlier version of
// this test tracked a `button_fired` bool that nothing ever assigned -- since
// button() installs its own listener -- so "the row did not fire" was
// satisfied by NOTHING firing and the positive half asserted nothing at all.
TEST(button_inside_a_clickable_row_receives_the_click) {
  ImmTestHarness h;

  bool row_fired = false;

  h.begin_frame();
  auto row = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}
                     .with_size(ComponentSize{pixels(200), pixels(40)})
                     .with_absolute_position(0.f, 0.f)
                     .with_debug_name("row"));
  row.ent().addComponentIfMissing<HasClickListener>(
      [&](Entity &) { row_fired = true; });
  auto btn_el = button(h.context(), mk(row.ent(), 0),
                       ComponentConfig{}
                           .with_size(ComponentSize{pixels(80), pixels(24)})
                           .with_label("X")
                           .with_debug_name("inner_button"));
  Entity &btn_ent = btn_el.ent();
  h.layout_only();
  mark_rendered();

  UIComponent *btn = h.find("inner_button");
  ui_test::check(btn != nullptr, "the button exists", __FILE__, __LINE__);
  if (!btn)
    return;

  // Press squarely in the middle of the button, which is also inside the row.
  press_at(h.context(), btn->rect().x + btn->rect().width * 0.5f,
           btn->rect().y + btn->rect().height * 0.5f);
  run_clicks(h);

  const bool button_fired = btn_ent.has<HasClickListener>() &&
                            btn_ent.get<HasClickListener>().down;
  ui_test::check(button_fired, "the button receives the click", __FILE__,
                 __LINE__);
  ui_test::check(!row_fired, "the row's listener does not fire", __FILE__,
                 __LINE__);
  fprintf(stderr, "        row_fired=%d button_fired=%d\n", (int)row_fired,
          (int)button_fired);
}

// Shape 2 (hanabi #3): absolutely-positioned siblings that overlap. The one
// drawn on top -- higher render layer -- should take the click.
TEST(topmost_of_two_overlapping_siblings_takes_the_click) {
  ImmTestHarness h;

  bool under_fired = false, over_fired = false;

  h.begin_frame();
  auto under = div(h.context(), mk(h.root(), 0),
                   ComponentConfig{}
                       .with_size(ComponentSize{pixels(200), pixels(80)})
                       .with_absolute_position(0.f, 0.f)
                       .with_debug_name("under"));
  under.ent().addComponentIfMissing<HasClickListener>(
      [&](Entity &) { under_fired = true; });

  // Same spot, drawn above.
  auto over = div(h.context(), mk(h.root(), 1),
                  ComponentConfig{}
                      .with_size(ComponentSize{pixels(40), pixels(40)})
                      .with_absolute_position(0.f, 0.f)
                      .with_render_layer(5)
                      .with_debug_name("over"));
  over.ent().addComponentIfMissing<HasClickListener>(
      [&](Entity &) { over_fired = true; });
  h.layout_only();
  mark_rendered();

  // A point covered by both.
  press_at(h.context(), 20.f, 20.f);
  run_clicks(h);

  ui_test::check(over_fired, "the element on top takes the click", __FILE__,
                 __LINE__);
  ui_test::check(!under_fired, "the element underneath does not", __FILE__,
                 __LINE__);
  fprintf(stderr, "        under_fired=%d over_fired=%d\n", (int)under_fired,
          (int)over_fired);
}

// Whatever the priority rule ends up being, exactly one handler may fire for
// one press. This is the "click consumption" half of the ask.
TEST(a_single_press_fires_exactly_one_listener) {
  ImmTestHarness h;

  int fire_count = 0;
  bool inner_fired = false;

  h.begin_frame();
  auto outer = div(h.context(), mk(h.root(), 0),
                   ComponentConfig{}
                       .with_size(ComponentSize{pixels(200), pixels(80)})
                       .with_absolute_position(0.f, 0.f)
                       .with_debug_name("outer"));
  outer.ent().addComponentIfMissing<HasClickListener>(
      [&](Entity &) { fire_count++; });
  auto inner = div(h.context(), mk(outer.ent(), 0),
                   ComponentConfig{}
                       .with_size(ComponentSize{pixels(60), pixels(30)})
                       .with_debug_name("inner"));
  inner.ent().addComponentIfMissing<HasClickListener>([&](Entity &) {
    fire_count++;
    inner_fired = true;
  });
  h.layout_only();
  mark_rendered();

  UIComponent *in = h.find("inner");
  if (!in)
    return;
  press_at(h.context(), in->rect().x + 5.f, in->rect().y + 5.f);
  run_clicks(h);

  ui_test::check(fire_count == 1, "exactly one listener fires per press",
                 __FILE__, __LINE__);
  // Which one matters as much as how many: a count of 1 is also what a
  // swallowed click looks like.
  ui_test::check(inner_fired, "and it is the inner one", __FILE__, __LINE__);
  fprintf(stderr, "        fire_count=%d inner_fired=%d\n", fire_count,
          (int)inner_fired);
}

// active is claimed on press and then deliberately persists across frames --
// BeginUIContextManager resets hot_id but not active_id. That is what keeps a
// press-and-hold (and any drag) alive after the cursor leaves the element, so
// the resolution pass must not hand active to whatever is under the cursor
// later in the gesture. Without the is_active(ROOT) guard, dragging across a
// UI would light up every control it passed over.
TEST(a_held_press_does_not_transfer_to_what_it_moves_over) {
  ImmTestHarness h;

  bool a_fired = false, b_fired = false;

  auto emit = [&] {
    auto a = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}
                     .with_size(ComponentSize{pixels(80), pixels(40)})
                     .with_absolute_position(0.f, 0.f)
                     .with_debug_name("a"));
    a.ent().addComponentIfMissing<HasClickListener>(
        [&](Entity &) { a_fired = true; });
    auto b = div(h.context(), mk(h.root(), 1),
                 ComponentConfig{}
                     .with_size(ComponentSize{pixels(80), pixels(40)})
                     .with_absolute_position(200.f, 0.f)
                     .with_debug_name("b"));
    b.ent().addComponentIfMissing<HasClickListener>(
        [&](Entity &) { b_fired = true; });
  };

  h.begin_frame(); emit(); h.layout_only(); mark_rendered();
  press_at(h.context(), 40.f, 20.f); // inside A
  run_clicks(h);
  ui_test::check(a_fired, "A fires on the press", __FILE__, __LINE__);

  // Still held, cursor now over B. press_at is not reused: it would reset
  // active to ROOT, which is exactly the state this test is about.
  a_fired = b_fired = false;
  h.begin_frame(); emit(); h.layout_only(); mark_rendered();
  h.context().mouse.pos = input::MousePosition{240.f, 20.f};
  h.context().mouse.just_pressed = false; // held, not a new press
  h.context().mouse.left_down = true;
  run_clicks(h);

  ui_test::check(!b_fired, "B does not fire under a press it never received",
                 __FILE__, __LINE__);
  ui_test::check(h.context().is_active(h.context().ROOT) == false,
                 "the gesture still owns active", __FILE__, __LINE__);
  fprintf(stderr, "        a_fired=%d b_fired=%d active=%d\n", (int)a_fired,
          (int)b_fired, (int)h.context().active_id);
}

int main() { return ui_test::run_registered_tests("hit priority"); }
