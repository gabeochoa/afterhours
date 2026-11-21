#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;
using ui_test::TestInputAction;

namespace {

struct Env {
  ImmTestHarness h;
  UIContext<TestInputAction> *ctx = nullptr;
  DragGroupState *state = nullptr;
  HandleDragGroupsPostLayout<TestInputAction> sys;
  std::vector<EntityID> rows;
  std::vector<std::string> model;

  Env() {
    EntityHelper::registerSingleton<UIContext<TestInputAction>>(
        h.context_entity());
    ctx = EntityHelper::get_singleton_cmp<UIContext<TestInputAction>>();
    Entity &state_entity = EntityHelper::createPermanentEntity();
    state = &state_entity.addComponent<DragGroupState>();
    EntityHelper::registerSingleton<DragGroupState>(state_entity);
  }

  void begin(bool scrolled) {
    sys.clear_all_drag_tags(state);
    imm::clear_existing_ui_elements();
    h.coll.cleanup();
    h.coll.merge_entity_arrays();
    state->reset_drag();
    state->events.clear();
    ctx->mouse.left_down = false;
    ctx->mouse.just_pressed = false;
    ctx->mouse.press_moved = false;
    ctx->last_action = TestInputAction::None;
    rows.clear();
    model = {"A", "B", "C", "D"};

    if (scrolled) {
      auto viewport = div(h.context(), mk(h.root(), 0),
                          ComponentConfig{}
                              .with_size({pixels(200), pixels(80)})
                              .with_absolute_position(0, 0)
                              .with_overflow(Overflow::Scroll));
      auto strip = div(h.context(), mk(viewport.ent(), 0),
                       ComponentConfig{}
                           .with_size({pixels(200), pixels(320)})
                           .with_flex_direction(FlexDirection::Column)
                           .with_draggable_children());
      for (int i = 0; i < 4; i++)
        rows.push_back(div(h.context(), mk(strip.ent(), i),
                           ComponentConfig{}.with_size(
                               {pixels(200), pixels(80)}))
                           .id());
      h.layout_only();
      auto &scroll = viewport.ent().get<HasScrollView>();
      scroll.content_size = {200, 320};
      scroll.viewport_size = Vector2Type{200, 80};
      return;
    }

    auto group = div(h.context(), mk(h.root(), 0),
                     ComponentConfig{}
                         .with_size({pixels(200), pixels(200)})
                         .with_absolute_position(0, 0)
                         .with_flex_direction(FlexDirection::Column)
                         .with_draggable_children());
    for (int i = 0; i < 4; i++)
      rows.push_back(div(h.context(), mk(group.ent(), i),
                         ComponentConfig{}.with_size(
                             {pixels(200), pixels(50)}))
                         .id());
    h.layout_only();
  }

  void frame(float x, float y, bool down, bool just, bool moved,
             float dt = 0.f) {
    ctx->mouse.pos = {x, y};
    ctx->mouse.left_down = down;
    ctx->mouse.just_pressed = just;
    ctx->mouse.press_moved = moved;
    sys.once(dt);
  }

  void escape_frame(float x, float y) {
    ctx->last_action = TestInputAction::MenuBack;
    frame(x, y, true, false, true);
  }

  void consume_events() {
    for (const auto &event : state->events) {
      const std::string moved = model[static_cast<size_t>(event.source_index)];
      model.erase(model.begin() + event.source_index);
      model.insert(model.begin() + event.target_index, moved);
    }
    state->events.clear();
  }

  bool row_hidden(EntityID id) {
    auto opt = ui::UICollectionHolder::getEntityForID(id);
    return opt.has_value() && opt.asE().get<UIComponent>().should_hide;
  }
};

Env &env() {
  static Env e;
  return e;
}

}

TEST(no_drag_below_threshold) {
  Env &f = env();
  f.begin(false);
  f.frame(100, 25, true, true, false);
  CHECK(f.state->armed);
  f.frame(100, 28, true, false, false);
  CHECK(f.state->armed);
  CHECK(!f.state->dragging);
  f.frame(100, 28, false, false, false);
  CHECK(!f.state->armed);
  CHECK(f.state->events.empty());
}

TEST(drag_first_row_to_third_reorders_model_on_drop) {
  Env &f = env();
  f.begin(false);
  f.frame(100, 25, true, true, false);
  f.frame(100, 140, true, false, true);
  CHECK(f.state->dragging);
  CHECK(f.state->hover_index == 2);
  CHECK(f.row_hidden(f.rows[0]));
  f.frame(100, 140, false, false, false);
  CHECK(!f.state->dragging);
  CHECK(f.state->events.size() == 1);
  if (f.state->events.size() == 1) {
    CHECK(f.state->events[0].source_index == 0);
    CHECK(f.state->events[0].target_index == 2);
  }
  f.consume_events();
  CHECK(f.model == std::vector<std::string>({"B", "C", "A", "D"}));
  CHECK(!f.row_hidden(f.rows[0]));
}

TEST(escape_cancels_drag_and_restores) {
  Env &f = env();
  f.begin(false);
  f.frame(100, 25, true, true, false);
  f.frame(100, 140, true, false, true);
  CHECK(f.state->dragging);
  CHECK(f.row_hidden(f.rows[0]));
  f.escape_frame(100, 140);
  CHECK(!f.state->dragging);
  CHECK(f.state->events.empty());
  CHECK(!f.row_hidden(f.rows[0]));
  f.consume_events();
  CHECK(f.model == std::vector<std::string>({"A", "B", "C", "D"}));
  f.frame(100, 160, true, false, true);
  CHECK(!f.state->dragging);
}

TEST(escape_disarms_before_threshold) {
  Env &f = env();
  f.begin(false);
  f.frame(100, 25, true, true, false);
  CHECK(f.state->armed);
  f.escape_frame(100, 25);
  CHECK(!f.state->armed);
  f.frame(100, 140, true, false, true);
  CHECK(!f.state->dragging);
}

TEST(edge_scroll_moves_viewport_while_dragging) {
  Env &f = env();
  f.begin(true);
  auto row_opt = ui::UICollectionHolder::getEntityForID(f.rows[0]);
  EntityID strip_id = row_opt.asE().get<UIComponent>().parent;
  auto strip_opt = ui::UICollectionHolder::getEntityForID(strip_id);
  EntityID view_id = strip_opt.asE().get<UIComponent>().parent;
  auto view_opt = ui::UICollectionHolder::getEntityForID(view_id);
  CHECK(view_opt.has_value());
  if (!view_opt.has_value())
    return;
  auto &scroll = view_opt.asE().get<HasScrollView>();
  CHECK_APPROX(scroll.scroll_offset.y, 0.f);

  f.frame(100, 40, true, true, false);
  f.frame(100, 75, true, false, true, 0.1f);
  CHECK(f.state->dragging);
  CHECK(scroll.scroll_offset.y > 0.f);
  CHECK_APPROX(scroll.scroll_offset.y, scroll.scroll_target.y);

  const float scrolled = scroll.scroll_offset.y;
  f.frame(100, 5, true, false, true, 0.1f);
  CHECK(scroll.scroll_offset.y < scrolled);
  CHECK(scroll.scroll_offset.y >= 0.f);
}

int main() { return ui_test::run_registered_tests("reorder drag tests"); }
