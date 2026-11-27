// Sticky children pin to the scroll viewport edge, release at parent end.
#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;
static void measure_and_stick(Entity &view) {
  MeasureScrollViews m;
  m.for_each_with(view, view.get<HasScrollView>(), view.get<UIComponent>(), 0.f);
  ApplyStickyChildren s;
  for (const auto &e : UICollectionHolder::get().collection.get_entities())
    if (e && e->has<HasSticky>())
      s.for_each_with(*e, e->get<HasSticky>(), e->get<UIComponent>(), 0.f);
}

static float screen_y(ImmTestHarness &h, const char *name, Entity &view) {
  UIComponent *c = h.find(name);
  return c ? c->rect().y - view.get<HasScrollView>().scroll_offset.y : -9999.f;
}

TEST(sticky_top_pins_and_child_moves_with_it) {
  ImmTestHarness h;
  h.begin_frame();
  auto view = div(h.context(), mk(h.root()),
                  ComponentConfig{}.with_size(ComponentSize{pixels(200.f), pixels(100.f)})
                      .with_overflow(Overflow::Scroll, Axis::Y)
                      .with_flex_direction(FlexDirection::Column).with_debug_name("view"));
  auto header = div(h.context(), mk(view.ent(), 1),
                    ComponentConfig{}.with_size(ComponentSize{percent(1.f), pixels(20.f)})
                        .with_sticky_top().with_debug_name("header"));
  div(h.context(), mk(header.ent(), 2),
      ComponentConfig{}.with_size(ComponentSize{percent(1.f), pixels(20.f)}).with_debug_name("header_child"));
  for (int i = 0; i < 10; i++)
    div(h.context(), mk(view.ent(), 10 + i),
        ComponentConfig{}.with_size(ComponentSize{percent(1.f), pixels(20.f)}).with_debug_name("row" + std::to_string(i)));
  h.layout_only();
  auto &sv = view.ent().get<HasScrollView>();
  sv.scroll_offset.y = sv.scroll_target.y = 60.f;
  measure_and_stick(view.ent());
  UIComponent *v = h.find("view");
  CHECK_APPROX(screen_y(h, "header", view.ent()), v->rect().y);
  CHECK_APPROX(screen_y(h, "header_child", view.ent()), v->rect().y);
  CHECK_APPROX(screen_y(h, "row5", view.ent()), v->rect().y + 60.f);
}

TEST(sticky_releases_at_parent_end) {
  ImmTestHarness h;
  h.begin_frame();
  auto view = div(h.context(), mk(h.root()),
                  ComponentConfig{}.with_size(ComponentSize{pixels(200.f), pixels(100.f)})
                      .with_overflow(Overflow::Scroll, Axis::Y)
                      .with_flex_direction(FlexDirection::Column).with_debug_name("view"));
  auto section = div(h.context(), mk(view.ent(), 1),
                     ComponentConfig{}.with_size(ComponentSize{percent(1.f), pixels(100.f)})
                         .with_flex_direction(FlexDirection::Column).with_debug_name("section"));
  div(h.context(), mk(section.ent(), 2),
      ComponentConfig{}.with_size(ComponentSize{percent(1.f), pixels(20.f)})
          .with_sticky_top().with_debug_name("header"));
  div(h.context(), mk(section.ent(), 3),
      ComponentConfig{}.with_size(ComponentSize{percent(1.f), pixels(80.f)}).with_debug_name("filler"));
  for (int i = 0; i < 10; i++)
    div(h.context(), mk(view.ent(), 10 + i),
        ComponentConfig{}.with_size(ComponentSize{percent(1.f), pixels(20.f)}).with_debug_name("row" + std::to_string(i)));
  h.layout_only();
  auto &sv = view.ent().get<HasScrollView>();
  sv.scroll_offset.y = sv.scroll_target.y = 150.f;
  measure_and_stick(view.ent());
  UIComponent *v = h.find("view");
  // Section bottom on screen is 100-150=-50; header (20h) rides it out.
  CHECK_APPROX(screen_y(h, "header", view.ent()), v->rect().y - 70.f);
}

TEST(sticky_bottom_pins) {
  ImmTestHarness h;
  h.begin_frame();
  auto view = div(h.context(), mk(h.root()),
                  ComponentConfig{}.with_size(ComponentSize{pixels(200.f), pixels(100.f)})
                      .with_overflow(Overflow::Scroll, Axis::Y)
                      .with_flex_direction(FlexDirection::Column).with_debug_name("view"));
  for (int i = 0; i < 10; i++)
    div(h.context(), mk(view.ent(), i),
        ComponentConfig{}.with_size(ComponentSize{percent(1.f), pixels(20.f)}).with_debug_name("row" + std::to_string(i)));
  div(h.context(), mk(view.ent(), 99),
      ComponentConfig{}.with_size(ComponentSize{percent(1.f), pixels(20.f)})
          .with_sticky_bottom().with_debug_name("footer"));
  h.layout_only();
  measure_and_stick(view.ent());
  UIComponent *v = h.find("view");
CHECK_APPROX(screen_y(h, "footer", view.ent()) + 20.f, v->rect().y + v->rect().height);
}

TEST(sticky_left_pins) {
  ImmTestHarness h;
  h.begin_frame();
  auto view = div(h.context(), mk(h.root()),
                  ComponentConfig{}.with_size(ComponentSize{pixels(200.f), pixels(100.f)})
                      .with_overflow(Overflow::Scroll, Axis::X)
                      .with_flex_direction(FlexDirection::Row).with_debug_name("view"));
  div(h.context(), mk(view.ent(), 1),
      ComponentConfig{}.with_size(ComponentSize{pixels(20.f), percent(1.f)})
          .with_sticky_left().with_debug_name("header"));
  for (int i = 0; i < 10; i++)
    div(h.context(), mk(view.ent(), 10 + i),
        ComponentConfig{}.with_size(ComponentSize{pixels(20.f), percent(1.f)}).with_debug_name("col" + std::to_string(i)));
  h.layout_only();
  auto &sv = view.ent().get<HasScrollView>();
  sv.scroll_offset.x = sv.scroll_target.x = 60.f;
  measure_and_stick(view.ent());
  UIComponent *v = h.find("view");
  UIComponent *hd = h.find("header");
  CHECK_APPROX(hd->rect().x - sv.scroll_offset.x, v->rect().x);
}

TEST(non_sticky_unchanged) {
  ImmTestHarness h;
  h.begin_frame();
  auto view = div(h.context(), mk(h.root()),
                  ComponentConfig{}.with_size(ComponentSize{pixels(200.f), pixels(100.f)})
                      .with_overflow(Overflow::Scroll, Axis::Y)
                      .with_flex_direction(FlexDirection::Column).with_debug_name("view"));
  div(h.context(), mk(view.ent(), 1),
      ComponentConfig{}.with_size(ComponentSize{percent(1.f), pixels(20.f)}).with_debug_name("plain"));
  h.layout_only();
  const float before = h.find("plain")->rect().y;
  auto &sv = view.ent().get<HasScrollView>();
  sv.scroll_offset.y = sv.scroll_target.y = 40.f;
  measure_and_stick(view.ent());
  CHECK_APPROX(h.find("plain")->rect().y, before);
}

int main() { return ui_test::run_registered_tests("sticky_children_test"); }
