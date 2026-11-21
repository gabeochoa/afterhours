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
struct RowStamp : BaseComponent { size_t key = 0; };
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

// ---------------------------------------------------------------------------
// The retained index (hanabi #420 remainder). Rebuilding the prefix meant
// asking height_of for every row every frame, and height_of is the expensive
// part (it wraps text). The library now keeps the heights on the list entity
// and only re-asks for rows the caller invalidated, or that are new.
// ---------------------------------------------------------------------------
TEST(retained_index_only_remeasures_dirty_rows) {
  ImmTestHarness h;
  int asked = 0;
  std::vector<size_t> asked_idx;
  auto height_of = [&](size_t i) {
    asked++;
    asked_idx.push_back(i);
    return alternating_height(i);
  };
  EntityID list_id = -1;
  auto frame = [&](size_t count) {
    h.begin_frame();
    auto list = virtual_list(
        h.context(), mk(h.root(), 0), count, height_of,
        [&](size_t, Entity &) {},
        ComponentConfig{}
            .with_size(ComponentSize{pixels(300), pixels(200)})
            .with_debug_name("vl_retained"));
    list_id = list.id();
    h.layout_only();
  };

  frame(100);
  CHECK(asked == 100); // first build measures everything once

  asked = 0;
  asked_idx.clear();
  frame(100);
  CHECK(asked == 0); // and never again while nothing changed

  invalidate_virtual_rows(AutoLayout::to_ent_static(list_id), 5, 8);
  frame(100);
  CHECK(asked == 3);
  CHECK(asked_idx == std::vector<size_t>({5, 6, 7}));

  asked = 0;
  frame(120); // growth measures only the new rows
  CHECK(asked == 20);

  asked = 0;
  frame(90); // shrink measures nothing
  CHECK(asked == 0);
}

// Prepending above the fold (hanabi #30a for virtual lists): the new rows'
// heights are added to the scroll position in the same build that measures
// them, so the row under the reader does not move.
TEST(prepend_virtual_rows_holds_the_viewport) {
  ImmTestHarness h;
  auto height_of = [](size_t i) { return (i % 2 == 0) ? 10.f : 40.f; };
  EntityID list_id = -1;
  auto frame = [&](size_t count) {
    h.begin_frame();
    auto list = virtual_list(
        h.context(), mk(h.root(), 0), count, height_of,
        [&](size_t, Entity &) {},
        ComponentConfig{}
            .with_size(ComponentSize{pixels(300), pixels(200)})
            .with_debug_name("vl_prepend"));
    list_id = list.id();
    h.layout_only();
  };

  frame(100);
  auto &scroll = AutoLayout::to_ent_static(list_id).get<HasScrollView>();
  scroll.viewport_size = {300.f, 200.f};
  // Row 20 starts at 10 * (10 + 40) = 500.
  CHECK_APPROX(virtual_row_offset(AutoLayout::to_ent_static(list_id), 20),
               500.f);
  scroll.scroll_offset.y = scroll.scroll_target.y = 500.f;
  scroll.last_eased_offset = scroll.scroll_offset;

  prepend_virtual_rows(AutoLayout::to_ent_static(list_id), 4);
  frame(104);
  auto &after = AutoLayout::to_ent_static(list_id).get<HasScrollView>();
  // Four new rows (10 + 40 + 10 + 40) now sit above the old row 20.
  CHECK_APPROX(after.scroll_offset.y, 600.f);
  CHECK_APPROX(after.scroll_target.y, 600.f);
  CHECK_APPROX(virtual_row_offset(AutoLayout::to_ent_static(list_id), 24),
               600.f);
}

static int count_uniform_built(bool use_default, size_t overscan) {
  ImmTestHarness h;
  int built = 0;
  auto config = ComponentConfig{}
                    .with_size(ComponentSize{pixels(300), pixels(200)})
                    .with_debug_name("vl_overscan");
  if (!use_default)
    config.with_virtual_list_overscan(overscan);
  virtual_list(h.context(), mk(h.root(), 0), 2000, 20.f,
               [&](size_t, Entity &) { built++; }, config);
  h.layout_only();
  return built;
}

static int count_measured_built(size_t overscan) {
  ImmTestHarness h;
  int built = 0;
  virtual_list(h.context(), mk(h.root(), 0), 2000, alternating_height,
               [&](size_t, Entity &) { built++; },
               ComponentConfig{}
                   .with_size(ComponentSize{pixels(300), pixels(200)})
                   .with_virtual_list_overscan(overscan)
                   .with_debug_name("vl_overscan_measured"));
  h.layout_only();
  return built;
}

TEST(virtual_list_overscan_defaults_to_four) {
  CHECK(ComponentConfig{}.virtual_list_overscan == 4);
  CHECK(count_uniform_built(true, 0) == 15);
  CHECK(count_uniform_built(false, 4) == 15);
}

TEST(virtual_list_overscan_controls_the_window) {
  CHECK(count_uniform_built(false, 0) == 11);
  CHECK(count_uniform_built(false, 1) == 12);
  CHECK(count_uniform_built(false, 10) == 21);
  CHECK(count_measured_built(0) < count_measured_built(4));
  CHECK(count_measured_built(4) < count_measured_built(20));
}

TEST(virtual_list_overscan_survives_apply_overrides) {
  CHECK(ComponentConfig{}
            .apply_overrides(
                ComponentConfig{}.with_virtual_list_overscan(9))
            .virtual_list_overscan == 9);
  CHECK(ComponentConfig{}
            .apply_overrides(
                ComponentConfig{}.with_virtual_list_overscan(0))
            .virtual_list_overscan == 0);
}

TEST(virtual_list_visible_range_at_offsets_and_boundaries) {
  ImmTestHarness h;
  EntityID list_id = -1;
  auto frame = [&](float offset) {
    h.begin_frame();
    auto list = virtual_list(h.context(), mk(h.root(), 0), 100, 20.f,
                             [&](size_t, Entity &) {},
                             ComponentConfig{}.with_size({pixels(300), pixels(200)}));
    list_id = list.id();
    h.layout_only();
    auto &sv = AutoLayout::to_ent_static(list_id).get<HasScrollView>();
    sv.viewport_size = {300.f, 200.f};
    sv.scroll_offset.y = sv.scroll_target.y = offset;
    sv.last_eased_offset = sv.scroll_offset;
  };
  auto range = [&] { return virtual_list_range(AutoLayout::to_ent_static(list_id)); };

  frame(0.f); frame(0.f);
  CHECK(range().visible_first == 0 && range().visible_last == 10);
  CHECK(range().first == 0 && range().last == 15);
  CHECK(range().count == 100 && !range().near_end(5));

  frame(20.f); frame(20.f);
  CHECK(range().visible_first == 1 && range().visible_last == 11);
  frame(25.f); frame(25.f);
  CHECK(range().visible_first == 1 && range().visible_last == 12);
  frame(200.f); frame(200.f);
  CHECK(range().visible_first == 10 && range().visible_last == 20);

  frame(1800.f); frame(1800.f);
  CHECK(range().visible_first == 90 && range().visible_last == 100);
  CHECK(range().last == 100 && range().near_end(5));
}

TEST(virtual_list_keyed_identity_survives_insert_and_remove_above) {
  ImmTestHarness h;
  std::vector<size_t> items{10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21};
  std::map<size_t, EntityID> row_for;
  std::map<size_t, size_t> stamp_for;
  auto frame = [&] {
    h.begin_frame();
    row_for.clear();
    virtual_list_keyed(
        h.context(), mk(h.root(), 0), items.size(), 20.f,
        [&](size_t i) { return items[i]; },
        [&](size_t i, Entity &row) {
          row_for[items[i]] = row.id;
          if (!row.has<RowStamp>())
            row.addComponent<RowStamp>().key = items[i];
        },
        ComponentConfig{}.with_size({pixels(300), pixels(200)}));
    h.layout_only();
  };

  frame();
  const auto before = row_for;
  for (const auto &[key, id] : before)
    stamp_for[key] = AutoLayout::to_ent_static(id).get<RowStamp>().key;
  CHECK(before.size() >= 10);

  items = {8, 9, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21};
  frame();
  for (const auto &[key, id] : before) {
    if (key == 10) { CHECK(row_for.count(key) == 0); continue; }
    if (!row_for.count(key)) continue;
    CHECK(row_for[key] == id);
    CHECK(AutoLayout::to_ent_static(id).get<RowStamp>().key == stamp_for[key]);
  }
  CHECK(row_for.count(8) && row_for.count(9));
  CHECK(row_for[8] != before.at(11) && row_for[9] != before.at(11));
}

int main() { return ui_test::run_registered_tests("virtual_list"); }
