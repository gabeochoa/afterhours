#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;
using ui_test::TestInputAction;

namespace {
void drag_frame(ImmTestHarness &h, HandleDragGroupsPostLayout<TestInputAction> &sys,
                float mx, bool down, bool just, bool moved) {
  h.context().mouse.pos = {mx, 10};
  h.context().mouse.left_down = down;
  h.context().mouse.just_pressed = just;
  h.context().mouse.press_moved = moved;
  sys.once(0.f);
}
} // namespace

TEST(drag_groups_hit_test_where_a_scrolled_strip_is_drawn) {
  ImmTestHarness h;
  EntityHelper::registerSingleton<UIContext<TestInputAction>>(h.context_entity());
  Entity &state_entity = EntityHelper::createPermanentEntity();
  auto &state = state_entity.addComponent<DragGroupState>();
  EntityHelper::registerSingleton<DragGroupState>(state_entity);

  auto viewport = div(h.context(), mk(h.root(), 0),
                      ComponentConfig{}
                          .with_size({pixels(200), pixels(80)})
                          .with_absolute_position(0, 0)
                          .with_overflow(Overflow::Scroll));
  auto strip = div(h.context(), mk(viewport.ent(), 0),
                   ComponentConfig{}
                       .with_size({pixels(400), pixels(80)})
                       .with_flex_direction(FlexDirection::Row)
                       .with_draggable_children());
  std::vector<EntityID> tabs;
  for (int i = 0; i < 4; i++)
    tabs.push_back(div(h.context(), mk(strip.ent(), i),
                       ComponentConfig{}.with_size({pixels(100), pixels(60)}))
                       .id());
  h.layout_only();
  viewport.ent().get<HasScrollView>().scroll_offset = {150, 0};
  HandleDragGroupsPostLayout<TestInputAction> sys;

  drag_frame(h, sys, 60, true, true, false);
  CHECK(state.armed && state.armed_item == tabs[2]);
  CHECK(std::abs(state.grab_offset_x - 10.f) < 0.01f);
  drag_frame(h, sys, 60, false, false, false);

  drag_frame(h, sys, 250, true, true, false);
  CHECK(!state.armed);
  drag_frame(h, sys, 250, false, false, false);

  drag_frame(h, sys, 60, true, true, false);
  drag_frame(h, sys, 120, true, false, true);
  CHECK(state.dragging && state.hover_index == 2);
  drag_frame(h, sys, 120, false, false, false);
}

int main() { return ui_test::run_registered_tests("drag group scroll hit tests"); }
