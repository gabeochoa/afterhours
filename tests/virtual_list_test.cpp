// virtual_list_test.cpp
// virtual_list divided by one row_height, so a list of measured rows could not
// use it. hanabi hand-rolled the same window three times rather than fight it,
// and a 2,000-row sidebar built 2,000 rows to show nineteen.
//
// The point of a windowed list is that it does NOT build what you cannot see,
// so that is what these assert: how many rows got built, not how they look.

#include "ui_test_harness.h"
#include <limits>

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

// ---------------------------------------------------------------------------
// The window (CP-006): bounded around the actual viewport, configurable
// overscan. The old range favoured a distant easing target and could drop the
// rows on screen during a scrollbar drag.
// ---------------------------------------------------------------------------
TEST(distant_scroll_targets_never_evict_the_visible_rows) {
  for (const float zoom : {1.f, 1.4f, 2.f}) {
    auto metrics = VirtualListMetrics::measured_rows(
        100000, [zoom](size_t row) { return zoom * (row % 7 == 0 ? 120.f : 18.f); });
    for (const float pixels : {0.f, 24.f, 96.f}) {
      size_t widest = 0;
      bool covered = true;
      for (size_t frame = 0; frame < 3000; ++frame) {
        const float offset = static_cast<float>((frame * 7919) % 2000000);
        const float target = frame % 2 ? 0.f : metrics.total();
        const auto range = virtual_list_window(metrics, offset, target, 600.f * zoom,
                                               VirtualListOptions{4, pixels * zoom, 3.f});
        covered = covered && range.first <= range.visible_first &&
                  range.last >= range.visible_last && range.last <= metrics.count &&
                  static_cast<long>(range.visible_first) == std::min<long>(
                      metrics.index_at(std::min(offset, metrics.total() - 600.f * zoom)),
                      static_cast<long>(metrics.count));
        widest = std::max(widest, range.last - range.first);
      }
      CHECK(covered);
      CHECK(widest < 130);
    }
  }
}

TEST(window_handles_empty_unknown_and_nonfinite_geometry) {
  CHECK(virtual_list_window(VirtualListMetrics::uniform_rows(0, 10.f), 0, 0, 100).last == 0);
  auto metrics = VirtualListMetrics::uniform_rows(100, 10.f);
  CHECK(virtual_list_window(metrics, 0, 0, 0).last == 1);
  auto range = virtual_list_window(metrics, std::numeric_limits<float>::quiet_NaN(), 0, 100);
  CHECK(range.first == 0);
  CHECK(range.last < 20);
  CHECK(VirtualListMetrics::measured_rows(3, [](size_t) {
          return std::numeric_limits<float>::infinity(); }).total() == 3.f);
}

TEST(pixel_overscan_builds_past_the_viewport) {
  const auto built_with = [](VirtualListOptions options) {
    ImmTestHarness h;
    int built = 0;
    virtual_list(h.context(), mk(h.root(), 0), 2000, 20.f, [&](size_t, Entity &) { built++; },
                 ComponentConfig{}.with_size({pixels(300), pixels(200)}), options);
    return built;
  };
  const int base = built_with({});
  CHECK(built_with({0, 0.f, 3.f}) == base - 4);
  CHECK(built_with({4, 100.f, 3.f}) == base + 5);
}

// ---------------------------------------------------------------------------
// keyed_virtual_list: the viewport and focus follow items, not positions.
// ---------------------------------------------------------------------------
namespace {
struct KeyedFixture {
  ImmTestHarness h;
  std::vector<std::string> items;
  std::uint64_t revision = 1;
  EntityID list = -1;
  std::vector<std::pair<std::string, EntityID>> buttons; // item -> its button
  float zoom = 1.f;

  void frame() {
    h.context().theme.ui_scale = zoom;
    h.begin_frame();
    buttons.clear();
    auto result = keyed_virtual_list(
        h.context(), mk(h.root(), 0), items.size(), revision,
        [&](size_t i) { return items[i]; },
        [&](size_t i) { return items[i].size() % 2 ? 20.f : 30.f; },
        [&](size_t i, Entity &row) {
          div(h.context(), mk(row, 0), ComponentConfig{}.with_size({pixels(10), pixels(5)}));
          auto button = div(h.context(), mk(row, 1),
                            ComponentConfig{}.with_size({pixels(10), pixels(5)}));
          buttons.emplace_back(items[i], button.ent().id);
        },
        ComponentConfig{}.with_size({pixels(300), pixels(200)})
            .with_padding(Padding{.top = pixels(7)})
            .with_scaling_mode(ScalingMode::Adaptive));
    list = result.id();
    layout_at_scale(h, zoom, false);
    auto &scroll = scroll_view();
    const auto &cmp = result.ent().get<UIComponent>();
    scroll.viewport_size = {cmp.rect().width, cmp.rect().height};
    scroll.content_size = measure_scroll_content(cmp, scroll);
  }
  HasScrollView &scroll_view() { return AutoLayout::to_ent_static(list).get<HasScrollView>(); }
  void scroll_to(float y) {
    auto &scroll = scroll_view();
    scroll.scroll_offset.y = scroll.scroll_target.y = scroll.last_eased_offset.y = y;
    frame();
  }
  // Distance from the viewport top to the top of `item`'s row, in pixels.
  float screen_top(const std::string &item) {
    const size_t i = static_cast<size_t>(std::find(items.begin(), items.end(), item) - items.begin());
    return 7.f * zoom + virtual_row_offset(AutoLayout::to_ent_static(list), i) -
           scroll_view().scroll_offset.y;
  }
  void update(std::vector<std::string> next) {
    items = std::move(next);
    revision++;
    frame();
  }
  EntityID button_of(const std::string &item) {
    for (auto &[key, id] : buttons)
      if (key == item) return id;
    return -1;
  }
};

std::vector<std::string> numbered(size_t from, size_t to) {
  std::vector<std::string> out;
  for (size_t i = from; i < to; i++) out.push_back("item" + std::to_string(i));
  return out;
}
} // namespace

TEST(keyed_list_keeps_the_top_item_through_insert_remove_and_empty) {
  for (const float zoom : {1.f, 1.4f, 2.f}) {
    KeyedFixture f;
    f.zoom = zoom;
    f.items = numbered(0, 300);
    f.frame();
    f.frame();
    f.scroll_to(f.screen_top("item50") + 11.f * zoom);
    const float before = f.screen_top("item50");
    CHECK_APPROX(before, -11.f * zoom);

    auto inserted = numbered(1000, 1007);
    inserted.insert(inserted.end(), f.items.begin(), f.items.end());
    f.update(inserted);
    CHECK_APPROX(f.screen_top("item50"), before);

    // Removing the anchored row brings up the row that followed it.
    auto removed = f.items;
    removed.erase(std::find(removed.begin(), removed.end(), "item50"));
    f.update(removed);
    CHECK_APPROX(f.screen_top("item51"), 0.f);

    // An empty filter keeps the anchor for the results that come back.
    f.update({});
    f.update(removed);
    CHECK_APPROX(f.screen_top("item51"), 0.f);
  }
}

TEST(keyed_list_focus_follows_its_item_to_a_recycled_row) {
  KeyedFixture f;
  f.items = numbered(0, 300);
  f.frame();
  f.frame();
  f.scroll_to(f.screen_top("item40"));
  const EntityID first = f.button_of("item42");
  f.h.context().set_focus(first);
  f.frame();

  // Reverse the order: item42 is now drawn by some other pooled row.
  auto reversed = f.items;
  std::reverse(reversed.begin(), reversed.end());
  f.update(reversed);
  const EntityID now = f.button_of("item42");
  CHECK(now >= 0);
  CHECK(f.h.context().focus_id == now);
  CHECK(now != first); // a different pooled entity, or this proves nothing

  // Focus moved by someone else is not dragged back.
  const EntityID other = f.button_of("item41");
  f.h.context().set_focus(other);
  f.update(numbered(0, 300));
  CHECK(f.h.context().focus_id == f.button_of("item41"));
}

int main() { return ui_test::run_registered_tests("virtual_list"); }
