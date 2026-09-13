#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;
using ui_test::TestInputAction;

static std::vector<DrawCall> render_entities(ImmTestHarness &h,
                                           const std::vector<EntityID> &ids,
                                           bool batched) {
  h.context().render_cmds.clear();
  for (auto id : ids) h.context().render_cmds.push_back({id, 1000});
  clear_draw_calls();
  auto &font = *h.render_font();
  if (batched) {
    RenderBatched<TestInputAction> renderer;
    renderer.for_each_with_derived(h.root(), h.context(), font, 0);
  } else {
    RenderImm<TestInputAction> renderer;
    renderer.for_each_with_derived(h.root(), h.context(), font, 0);
  }
  return draw_calls();
}

static ComponentConfig positioned(float x, float y, float w, float height) {
  return ComponentConfig{}.with_size({pixels(w), pixels(height)})
      .with_absolute_position(x, y).with_corner_radius(0)
      .with_skip_grid_snap(true).with_transparent_bg();
}

static void preview_matches_source(bool batched, bool faded_parent) {
  ImmTestHarness h;
  int clicked = 0;
  auto viewport = div(h.context(), mk(h.root(), 0), positioned(20, 30, 360, 180)
      .with_opacity(faded_parent ? .5f : 1.f).with_overflow(Overflow::Scroll));
  auto card = button(h.context(), mk(viewport.ent(), 0), positioned(40, 120, 220, 96)
      .with_custom_background({31, 82, 133, 255}).with_border({220, 170, 70, 255}, 2)
      .with_corner_radius(9).with_on_draw_fg([](RectangleType r) {
        draw_rectangle({r.x + 180, r.y + 12, 20, 20}, {140, 200, 100, 255});
      }));
  card.ent().get<HasClickListener>().cb = [&](Entity &) { ++clicked; };
  auto title = div(h.context(), mk(card.ent(), 0), positioned(12, 12, 154, 30)
      .with_label("Card title").with_font(UIComponent::DEFAULT_FONT, pixels(18))
      .with_custom_text_color({245, 240, 230, 255}));
  auto badge = div(h.context(), mk(card.ent(), 1), positioned(12, 55, 80, 24)
      .with_custom_background({130, 43, 78, 255}).with_corner_radius(4)
      .with_label("Design").with_font(UIComponent::DEFAULT_FONT, pixels(14))
      .with_custom_text_color({240, 240, 240, 255}));
  h.layout_only();
  viewport.ent().get<HasScrollView>().scroll_offset = {0, 64};
  const auto card_before = card.cmp().computed_rel;
  const auto title_before = title.cmp().computed_rel;
  const auto badge_before = badge.cmp().computed_rel;
  const auto parent_before = card.cmp().parent;
  const bool had_opacity = card.ent().has<HasOpacity>();
  const float opacity_before = had_opacity ? card.ent().get<HasOpacity>().value : 1.f;
  const auto baseline = render_entities(h, {card.id(), title.id(), badge.id()}, batched);
  CHECK(baseline.size() >= 5);
  CHECK(std::any_of(baseline.begin(), baseline.end(), [](const DrawCall &call) {
    return call.text == "Card title";
  }));
  card.ent().enableTag(DragTag::DraggedItem);
  card.cmp().should_hide = true;
  CHECK(render_entities(h, {card.id(), title.id(), badge.id()}, batched).empty());
  DragGroupState state;
  state.dragged_width = 220;
  state.dragged_height = 96;
  create_or_update_drag_overlay(state, 530, 378);
  h.coll.merge_entity_arrays();
  auto preview = find_drag_tagged(DragTag::Overlay);
  CHECK(preview.valid());
  if (!preview) return;
  CHECK(!preview.asE().has<HasClickListener>());
  CHECK(!preview.asE().has<HasLabel>());
  CHECK(preview.asE().get<UIComponent>().children.empty());
  const auto copied = render_entities(h, {preview.asE().id}, batched);
  CHECK(copied.size() == baseline.size());
  CHECK(render_entities(h, {card.id(), title.id(), badge.id(), preview.asE().id},
                        batched).size() == copied.size());
  CHECK(render_entities(h, {preview.asE().id, card.id(), title.id(), badge.id()},
                        batched).size() == copied.size());
  const float dx = 420 - card_before[Axis::X];
  const float dy = 330 - (card_before[Axis::Y] - 64);
  for (size_t i = 0; i < std::min(copied.size(), baseline.size()); ++i) {
    CHECK(copied[i].op == baseline[i].op);
    CHECK(copied[i].text == baseline[i].text);
    CHECK_APPROX(copied[i].rect.x, baseline[i].rect.x + dx);
    CHECK_APPROX(copied[i].rect.y, baseline[i].rect.y + dy);
    CHECK_APPROX(copied[i].rect.width, baseline[i].rect.width);
    CHECK_APPROX(copied[i].rect.height, baseline[i].rect.height);
    CHECK(copied[i].color.r == baseline[i].color.r);
    CHECK(copied[i].color.g == baseline[i].color.g);
    CHECK(copied[i].color.b == baseline[i].color.b);
    CHECK(copied[i].color.a == baseline[i].color.a);
  }
  CHECK(card.cmp().parent == parent_before);
  CHECK(card.ent().has<HasClickListener>());
  CHECK(card.ent().has<HasOpacity>() == had_opacity);
  if (had_opacity) CHECK_APPROX(card.ent().get<HasOpacity>().value, opacity_before);
  CHECK(card.cmp().should_hide);
  CHECK_APPROX(card.cmp().computed_rel[Axis::X], card_before[Axis::X]);
  CHECK_APPROX(card.cmp().computed_rel[Axis::Y], card_before[Axis::Y]);
  CHECK_APPROX(title.cmp().computed_rel[Axis::X], title_before[Axis::X]);
  CHECK_APPROX(title.cmp().computed_rel[Axis::Y], title_before[Axis::Y]);
  CHECK_APPROX(badge.cmp().computed_rel[Axis::X], badge_before[Axis::X]);
  CHECK_APPROX(badge.cmp().computed_rel[Axis::Y], badge_before[Axis::Y]);
  CHECK(clicked == 0);
  CHECK(!h.context().focused_ids.contains(preview.asE().id));
}

TEST(drag_preview_matches_complete_card_in_both_renderers) {
  preview_matches_source(false, false);
  preview_matches_source(true, false);
}

TEST(hidden_ancestors_suppress_descendants_in_both_renderers) {
  for (const bool batched : {false, true}) {
    ImmTestHarness h;
    auto parent = div(h.context(), mk(h.root(), 0), positioned(20, 30, 220, 120));
    auto middle = div(h.context(), mk(parent.ent(), 0), positioned(10, 10, 200, 100));
    auto child = button(h.context(), mk(middle.ent(), 0), positioned(10, 10, 180, 40)
        .with_label("Visible descendant").with_font(UIComponent::DEFAULT_FONT, pixels(18))
        .with_custom_background({31, 82, 133, 255}));
    h.layout_only();
    h.context().visual_focus_id = child.id();
    const auto visible = render_entities(h, {child.id()}, batched);
    CHECK(!visible.empty());
    parent.cmp().should_hide = true;
    CHECK(render_entities(h, {child.id()}, batched).empty());
    parent.cmp().should_hide = false;
    parent.ent().addComponent<ShouldHide>();
    CHECK(render_entities(h, {child.id()}, batched).empty());
    parent.ent().removeComponent<ShouldHide>();
    CHECK(render_entities(h, {child.id()}, batched).size() == visible.size());
  }
}

TEST(drag_preview_retains_inherited_opacity) {
  preview_matches_source(false, true);
  preview_matches_source(true, true);
}

TEST(drag_preview_skips_hidden_subtrees_and_keeps_internal_clip) {
  ImmTestHarness h;
  auto card = div(h.context(), mk(h.root(), 0), positioned(20, 30, 200, 120)
      .with_custom_background({31, 82, 133, 255}).with_overflow(Overflow::Hidden));
  auto child = div(h.context(), mk(card.ent(), 0), positioned(160, 20, 100, 30)
      .with_custom_background({200, 70, 50, 255}));
  auto hidden = div(h.context(), mk(card.ent(), 1), positioned(10, 60, 120, 40));
  auto hidden_child = div(h.context(), mk(hidden.ent(), 0), positioned(0, 0, 80, 30)
      .with_label("Hidden child").with_font(UIComponent::DEFAULT_FONT, pixels(16)));
  h.layout_only();
  hidden.ent().addComponent<ShouldHide>();
  auto overlay = div(h.context(), mk(h.root(), 1), positioned(420, 330, 200, 120));
  h.layout_only();
  overlay.ent().addComponent<HasDragPreview>(card.id());
  card.cmp().should_hide = true;
  auto &fonts = *h.render_font();
  Arena arena(Arena::DEFAULT_CAPACITY);
  RenderCommandBuffer buffer(arena);
  RenderBatched<TestInputAction> renderer;
  renderer.collect(buffer, h.context(), fonts, overlay.ent(), 1000);
  bool saw_child = false;
  bool saw_clip = false;
  for (const auto &command : buffer.commands()) {
    CHECK(command.entity_id != hidden_child.id());
    if (command.entity_id == child.id()) {
      if (command.type == RenderPrimitiveType::RoundedRectangle) {
        saw_child = true;
        CHECK_APPROX(command.data.rectangle.roundness, 0);
        CHECK_APPROX(command.data.rectangle.rect.x, 580);
        CHECK_APPROX(command.data.rectangle.rect.y, 350);
        CHECK_APPROX(command.data.rectangle.rect.width, 100);
        CHECK_APPROX(command.data.rectangle.rect.height, 30);
        CHECK(command.data.rectangle.fill_color.r == 200);
        CHECK(command.data.rectangle.fill_color.g == 70);
        CHECK(command.data.rectangle.fill_color.b == 50);
      }
      if (command.type == RenderPrimitiveType::ScissorStart) {
        saw_clip = true;
        CHECK(command.data.scissor.x == 420);
        CHECK(command.data.scissor.y == 330);
        CHECK(command.data.scissor.width == 200);
        CHECK(command.data.scissor.height == 120);
      }
    }
  }
  CHECK(saw_child);
  CHECK(saw_clip);
  CHECK(hidden.ent().has<ShouldHide>());
  CHECK_APPROX(child.cmp().rect().x, 180);
  CHECK_APPROX(child.cmp().rect().y, 50);
  card.ent().addComponent<ShouldHide>();
  const auto calls = render_entities(h, {overlay.id()}, true);
  CHECK(calls.empty());
}

TEST(skipped_grid_snap_keeps_centered_and_trailing_cards_adjacent) {
  for (const auto direction : {FlexDirection::Row, FlexDirection::Column}) {
    for (const auto justify : {JustifyContent::Center, JustifyContent::FlexEnd}) {
      for (const bool skip_parent : {false, true}) {
        ImmTestHarness h;
        auto row = div(h.context(), mk(h.root(), 0), ComponentConfig{}
            .with_size({pixels(184), pixels(184)}).with_flex_direction(direction)
            .with_no_wrap().with_justify_content(justify).with_skip_grid_snap(skip_parent));
        std::vector<EntityID> items;
        for (int i = 0; i < 3; ++i)
          items.push_back(div(h.context(), mk(row.ent(), i), ComponentConfig{}
              .with_size({pixels(30), pixels(30)}).with_skip_grid_snap(!skip_parent)).id());
        h.layout_only(true, {1280, 720});
        const Axis axis = direction == FlexDirection::Row ? Axis::X : Axis::Y;
        const float start = justify == JustifyContent::Center ? 47 : 94;
        for (size_t i = 0; i < items.size(); ++i) {
          const auto &cmp = AutoLayout::to_cmp_static(items[i]);
          CHECK(std::abs(cmp.computed_rel[axis] - start - static_cast<float>(i) * 30) < .01f);
          CHECK_APPROX(cmp.computed[axis], 30);
        }
      }
    }
  }
}

TEST(changing_overflow_releases_and_restores_clipping) {
  ImmTestHarness h;
  const auto frame = [&](Overflow overflow) {
    h.begin_frame();
    auto parent = div(h.context(), mk(h.root(), 0), positioned(100, 100, 200, 120)
        .with_overflow(overflow));
    auto child = div(h.context(), mk(parent.ent(), 0), positioned(180, 10, 60, 40));
    h.layout_only();
    const auto [clipped, rect] = ui::detail::compute_intersected_clip_rect(child.ent());
    CHECK(clipped == (overflow == Overflow::Hidden));
    CHECK(parent.ent().has<HasClipChildren>() == (overflow == Overflow::Hidden));
    if (clipped) {
      CHECK_APPROX(rect.x, 100);
      CHECK_APPROX(rect.y, 100);
      CHECK_APPROX(rect.width, 200);
      CHECK_APPROX(rect.height, 120);
    }
    return parent.id();
  };
  const auto id = frame(Overflow::Hidden);
  frame(Overflow::Visible);
  frame(Overflow::Hidden);
  auto &parent = AutoLayout::to_ent_static(id);
  ui::imm::detail::apply_restyle(h.context(), parent,
      ComponentConfig{}.with_custom_background({20, 30, 40, 255}));
  CHECK(parent.has<HasClipChildren>());
}

int main() { return ui_test::run_registered_tests("drag preview and exact layout tests"); }
