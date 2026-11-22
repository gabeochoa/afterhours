#include "ui_test_harness.h"
#include <afterhours/src/plugins/ui/tooltip.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

namespace {
TooltipState &ensure_state() {
  if (!EntityHelper::has_singleton<TooltipState>()) {
    auto &e = EntityHelper::createPermanentEntity();
    e.addComponent<TooltipState>();
    EntityHelper::registerSingleton<TooltipState>(e);
  }
  auto &s = *EntityHelper::get_singleton_cmp<TooltipState>();
  s.hovered = -1;
  s.showing = -1;
  s.elapsed = 0.f;
  s.text.clear();
  return s;
}
}

TEST(focus_shows_tooltip_immediately_and_dismisses_on_leave) {
  ui_test::ImmTestHarness h;
  auto &state = ensure_state();
  h.begin_frame();
  auto w = div(h.context(), mk(h.root(), 0),
               ComponentConfig{}.with_size({pixels(40), pixels(40)})
                   .with_tooltip("hint", 5.f));
  h.context().hot_id = -1;
  h.context().set_focus(w.ent().id);
  UpdateTooltips<ui_test::TestInputAction> update;
  update.for_each_with(h.context_entity(), h.context(), 0.f);
  CHECK(state.is_showing());
  CHECK(state.text == "hint");
  h.context().set_focus(-1);
  update.for_each_with(h.context_entity(), h.context(), 0.f);
  CHECK(!state.is_showing());
}

TEST(placement_setter_reaches_state_and_flips_at_screen_edge) {
  ui_test::ImmTestHarness h;
  auto &state = ensure_state();
  h.begin_frame();
  auto w = div(h.context(), mk(h.root(), 0),
               ComponentConfig{}.with_size({pixels(40), pixels(40)})
                   .with_tooltip("hint", 0.f)
                   .with_tooltip_placement(overlay::Placement::Right));
  CHECK(w.ent().get<HasTooltip>().placement == overlay::Placement::Right);
  h.context().hot_id = w.ent().id;
  UpdateTooltips<ui_test::TestInputAction> update;
  update.for_each_with(h.context_entity(), h.context(), 0.f);
  update.for_each_with(h.context_entity(), h.context(), 0.f);
  CHECK(state.placement == overlay::Placement::Right);

  if (!EntityHelper::has_singleton<FontManager>()) {
    auto &fe = EntityHelper::createPermanentEntity();
    fe.addComponent<FontManager>().load_font("Interface", get_default_font());
    EntityHelper::registerSingleton<FontManager>(fe);
  } else {
    h.render_font();
  }
  state.showing = w.ent().id;
  state.anchor = {700, 100, 60, 30};
  state.text = "hint";
  state.font_size = std::nullopt;
  state.padding = 8.f;
  state.gap = 4.f;
  set_measure_text_fn([](const char *, float size, float) {
    return Vector2Type{100.f, size};
  });
  clear_draw_calls();
  RenderTooltip<ui_test::TestInputAction> renderer;
  renderer.for_each_with(h.context_entity(), h.context(), 0.f);
  const auto boxes = h.drawn("rectangle_rounded");
  CHECK(boxes.size() == 1);
  if (!boxes.empty()) CHECK(boxes.front().rect.x < 700.f);
}

TEST(rebuild_refreshes_text_placement_anchor_and_clears) {
  ui_test::ImmTestHarness h;
  auto &state = ensure_state();
  auto emit = [&](const std::string &text, overlay::Placement p, float x) {
    h.begin_frame();
    auto w = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}.with_size({pixels(40), pixels(40)})
                     .with_absolute_position(x, 100)
                     .with_tooltip(text, 0.f)
                     .with_tooltip_placement(p));
    h.layout_only();
    return w;
  };
  auto first = emit("first", overlay::Placement::Below, 100);
  const EntityID id = first.ent().id;
  h.context().hot_id = id;
  UpdateTooltips<ui_test::TestInputAction> update;
  update.for_each_with(h.context_entity(), h.context(), 0.f);
  update.for_each_with(h.context_entity(), h.context(), 0.f);
  CHECK(state.text == "first");
  const float old_x = state.anchor.x;

  auto second = emit("second", overlay::Placement::Above, 200);
  CHECK(second.ent().id == id);
  CHECK(second.ent().get<HasTooltip>().text == "second");
  CHECK(second.ent().get<HasTooltip>().placement == overlay::Placement::Above);
  update.for_each_with(h.context_entity(), h.context(), 0.f);
  CHECK(state.text == "second");
  CHECK(state.placement == overlay::Placement::Above);
  CHECK(state.anchor.x != old_x);

  auto cleared = emit("", overlay::Placement::Below, 200);
  CHECK(cleared.ent().id == id);
  CHECK(!cleared.ent().has<HasTooltip>());
  update.for_each_with(h.context_entity(), h.context(), 0.f);
  CHECK(!state.is_showing());
}

int main() { return ui_test::run_registered_tests("tooltip"); }
