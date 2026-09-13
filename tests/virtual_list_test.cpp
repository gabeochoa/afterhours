// virtual_list_test.cpp
// virtual_list divided by one row_height, so a list of measured rows could not
// use it. hanabi hand-rolled the same window three times rather than fight it,
// and a 2,000-row sidebar built 2,000 rows to show nineteen.
//
// The point of a windowed list is that it does NOT build what you cannot see,
// so that is what these assert: how many rows got built, not how they look.

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
// Rows alternate short and tall, so a uniform assumption cannot be right for
// both and the window has to come from the running total.
float alternating_height(size_t i) { return (i % 2 == 0) ? 10.f : 40.f; }
} // namespace

TEST(uniform_rows_build_only_the_window) {
  ImmTestHarness h;
  int built = 0;
  virtual_list(
      h.context(), mk(h.root(), 0), 2000, 20.f,
      [&](size_t, Entity &) { built++; },
      ComponentConfig{}
          .with_size(ComponentSize{pixels(300), pixels(200)})
          .with_debug_name("vl"));
  h.layout_only();

  printf("  uniform: built %d of 2000\n", built);
  CHECK(built > 0);
  CHECK(built < 100); // a window, not the list
}

TEST(measured_rows_build_only_the_window) {
  ImmTestHarness h;
  int built = 0;
  virtual_list(
      h.context(), mk(h.root(), 0), 2000, alternating_height,
      [&](size_t, Entity &) { built++; },
      ComponentConfig{}
          .with_size(ComponentSize{pixels(300), pixels(200)})
          .with_debug_name("vl_measured"));
  h.layout_only();

  printf("  measured: built %d of 2000\n", built);
  CHECK(built > 0);
  CHECK(built < 100);
}

// The rows it does build have to be their own heights, or the window is right
// and the list still looks wrong.
TEST(measured_rows_get_their_own_heights) {
  ImmTestHarness h;
  std::vector<float> heights;
  virtual_list(
      h.context(), mk(h.root(), 0), 12, alternating_height,
      [&](size_t i, Entity &row) {
        (void)i;
        heights.push_back(row.get<UIComponent>().desired[Axis::Y].value);
      },
      ComponentConfig{}
          .with_size(ComponentSize{pixels(300), pixels(200)})
          .with_debug_name("vl_heights"));
  h.layout_only();

  CHECK(heights.size() >= 2);
  bool saw_short = false, saw_tall = false;
  for (float ht : heights) {
    saw_short |= (ht == 10.f);
    saw_tall |= (ht == 40.f);
  }
  printf("  heights: %zu rows, short=%d tall=%d\n", heights.size(),
         (int)saw_short, (int)saw_tall);
  CHECK(saw_short);
  CHECK(saw_tall);
}

static void layout_at_scale(ImmTestHarness &h, float scale, bool grid) {
  h.coll.merge_entity_arrays();
  std::vector<Entity *> mapping;
  for (const auto &entity : h.coll.get_entities()) {
    if (!entity) continue;
    mapping.resize(std::max(mapping.size(), static_cast<size_t>(entity->id) + 1));
    mapping[static_cast<size_t>(entity->id)] = entity.get();
  }
  AutoLayout::autolayout(h.root().get<UIComponent>(), {800, 600}, mapping, grid, scale);
}

static void check_virtual_geometry(float zoom, bool grid, bool variable,
                                   ScalingMode screen_mode,
                                   std::optional<ScalingMode> list_override,
                                   int viewport_kind) {
  ImmTestHarness h;
  h.context().theme.ui_scale = zoom;
  h.context().scaling_mode = screen_mode;
  const auto mode = list_override.value_or(screen_mode);
  const float scale = mode == ScalingMode::Adaptive ? zoom : 1.f;
  const float gap = 5.f * scale;
  const auto height = [&](size_t index) {
    constexpr float sizes[] = {13.f, 29.f, 41.f};
    return (variable ? sizes[index % 3] : 26.f) * scale;
  };
  const auto offset = [&](size_t index) {
    float value = static_cast<float>(index) * gap;
    for (size_t i = 0; i < index; ++i) value += height(i);
    return value;
  };
  EntityID list_id = -1;
  std::vector<std::pair<size_t, EntityID>> built;
  const auto frame = [&](size_t count) {
    h.begin_frame();
    const auto parent_mode = screen_mode == ScalingMode::Adaptive
        ? ScalingMode::Proportional : ScalingMode::Adaptive;
    const float parent_scale = parent_mode == ScalingMode::Adaptive ? zoom : 1.f;
    auto parent = div(h.context(), mk(h.root(), 0), ComponentConfig{}
        .with_size({pixels(400 / parent_scale), pixels(400 / parent_scale)})
        .with_scaling_mode(parent_mode));
    auto config = ComponentConfig{}.with_size({percent(1.f),
        viewport_kind == 2 ? h720(200) : viewport_kind == 1 ? percent(.5f) : pixels(200)})
        .with_gap(pixels(5))
        .with_padding(Padding{.top = pixels(13), .bottom = pixels(17)})
        .with_justify_content(JustifyContent::Center);
    if (list_override) config.with_scaling_mode(*list_override);
    built.clear();
    const auto render = [&](size_t index, Entity &row) { built.emplace_back(index, row.id); };
    auto list = variable
        ? virtual_list(h.context(), mk(parent.ent(), 0), count,
            [&](size_t i) { return height(i) / scale; }, render, config)
        : virtual_list(h.context(), mk(parent.ent(), 0), count, 26.f, render, config);
    list_id = list.id();
    layout_at_scale(h, zoom, grid);
    auto &cmp = list.ent().get<UIComponent>();
    auto &scroll = list.ent().get<HasScrollView>();
    scroll.viewport_size = {cmp.rect().width, cmp.rect().height};
    scroll.content_size = measure_scroll_content(cmp, scroll);
    const float total = count ? offset(count) - gap : 0.f;
    CHECK(std::abs(scroll.content_size.y - total - 30.f * scale) < .05f);
    CHECK(built.size() <= 3 * static_cast<size_t>(std::ceil(cmp.rect().height / (13.f * scale))) + 10);
    for (const auto &[index, id] : built) {
      const auto &row = AutoLayout::to_cmp_static(id);
      CHECK(row.resolved_scaling_mode == mode);
      CHECK(std::abs(row.rect().height - height(index)) < .05f);
      CHECK(std::abs(row.rect().y - cmp.rect().y - 13.f * scale - offset(index)) < .05f);
    }
    for (size_t index = 0; index < count; ++index) {
      const float top = 13.f * scale + offset(index) - scroll.scroll_offset.y;
      if (top + height(index) <= 0 || top >= cmp.rect().height) continue;
      CHECK(std::any_of(built.begin(), built.end(), [&](const auto &row) { return row.first == index; }));
    }
  };
  frame(99);
  frame(99);
  for (const float target : {13.f * scale + offset(50), 100000.f, 0.f}) {
    auto &scroll = AutoLayout::to_ent_static(list_id).get<HasScrollView>();
    scroll.scroll_offset.y = target;
    scroll.scroll_target.y = target;
    frame(99);
    if (target != 100000.f) continue;
    CHECK(!built.empty() && built.back().first == 98);
    const auto &list = AutoLayout::to_ent_static(list_id);
    const auto &row = AutoLayout::to_cmp_static(built.back().second);
    CHECK(std::abs(row.rect().y + row.rect().height - list.get<UIComponent>().rect().y -
        scroll.scroll_offset.y - scroll.viewport_size->y + 17.f * scale) < .05f);
  }
  auto &scroll = AutoLayout::to_ent_static(list_id).get<HasScrollView>();
  scroll.scroll_target.y = scroll.scroll_offset.y = 100000.f;
  frame(3);
  CHECK(built.size() == 3);
  frame(0);
  CHECK(built.empty());
  CHECK(scroll.scroll_offset.y == 0.f);
  CHECK(scroll.scroll_target.y == 0.f);
  CHECK(scroll.unbuilt_content_size.y == 0.f);
}

TEST(row_geometry_matches_scroll_units_across_zoom_and_overrides) {
  for (const float zoom : {1.f, 1.4f, 2.f}) {
    for (const bool grid : {false, true}) {
      for (const bool variable : {false, true}) {
        check_virtual_geometry(zoom, grid, variable, ScalingMode::Adaptive, std::nullopt, false);
        check_virtual_geometry(zoom, grid, variable, ScalingMode::Proportional, std::nullopt, false);
        check_virtual_geometry(zoom, grid, variable, ScalingMode::Proportional, ScalingMode::Adaptive, false);
        check_virtual_geometry(zoom, grid, variable, ScalingMode::Adaptive, ScalingMode::Proportional, false);
        check_virtual_geometry(zoom, grid, variable, ScalingMode::Adaptive, std::nullopt, true);
        check_virtual_geometry(zoom, grid, variable, ScalingMode::Adaptive, std::nullopt, 2);
      }
    }
  }
}

TEST(reused_list_recomputes_metrics_when_zoom_or_mode_changes) {
  ImmTestHarness h;
  EntityID list_id = -1;
  for (const auto mode : {ScalingMode::Adaptive, ScalingMode::Proportional, ScalingMode::Adaptive}) {
    for (const float zoom : {1.f, 2.f, 1.4f}) {
      h.context().theme.ui_scale = zoom;
      h.begin_frame();
      if (list_id >= 0) {
        auto &scroll = AutoLayout::to_ent_static(list_id).get<HasScrollView>();
        scroll.scroll_offset.y = scroll.scroll_target.y = 900.f;
      }
      std::vector<std::pair<size_t, EntityID>> rows;
      auto list = virtual_list(h.context(), mk(h.root(), 0), 100, 26.f,
          [&](size_t index, Entity &row) { rows.emplace_back(index, row.id); },
          ComponentConfig{}.with_size({pixels(300), pixels(200)}).with_scaling_mode(mode));
      list_id = list.id();
      layout_at_scale(h, zoom, true);
      const float scale = mode == ScalingMode::Adaptive ? zoom : 1.f;
      const auto &cmp = list.ent().get<UIComponent>();
      auto &scroll = list.ent().get<HasScrollView>();
      scroll.viewport_size = {cmp.rect().width, cmp.rect().height};
      CHECK(std::abs(measure_scroll_content(cmp, scroll).y - 2600.f * scale) < .05f);
      for (const auto &[index, id] : rows) {
        const auto &row = AutoLayout::to_cmp_static(id);
        CHECK(std::abs(row.rect().height - 26.f * scale) < .05f);
        CHECK(std::abs(row.rect().y - cmp.rect().y - static_cast<float>(index) * 26.f * scale) < .05f);
      }
    }
  }
}

int main() { return ui_test::run_registered_tests("virtual_list"); }
