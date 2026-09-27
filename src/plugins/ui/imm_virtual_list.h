#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <fstream>
#include <string>
#include <utility>

#include "../../ecs.h"
#include "overlay.h"
#include "../input_system.h"
#include "component_init.h"
#include "components.h"
#include "element_result.h"
#include "measure_config.h"
#include "entity_management.h"
#include "fmt/format.h"
#include "rendering.h"
#include "rounded_corners.h"

#include "imm_primitives.h"

namespace afterhours {

namespace ui {

namespace imm {

namespace detail {
// Row geometry for a windowed list. Uniform rows keep the arithmetic O(1);
// measured rows carry a prefix sum and binary-search it. Every list in a real
// app has differing heights, so hanabi hand-rolled this window three times
// before it lived here.
struct RowMetrics {
  float uniform = 0.f;      // used when prefix is empty
  std::vector<float> prefix; // prefix[i] = height of rows [0, i)
  // Set instead of prefix when the retained index owns the sums.
  const std::vector<float> *borrowed = nullptr;
  HasVirtualListIndex *index = nullptr;
  size_t count = 0;
  float scale = 1.f;
  float gap = 0.f;

  static RowMetrics uniform_rows(size_t n, float h) {
    RowMetrics m;
    m.uniform = h > 0.f ? h : 1.f;
    m.count = n;
    return m;
  }

  [[nodiscard]] bool is_uniform() const {
    return prefix.empty() && !borrowed;
  }
  [[nodiscard]] float height_of(size_t i) const {
    if (is_uniform())
      return uniform * scale;
    const std::vector<float> &p = borrowed ? *borrowed : prefix;
    return (p[i + 1] - p[i]) * scale;
  }
  [[nodiscard]] float offset_of(size_t i) const {
    if (is_uniform())
      return (uniform * static_cast<float>(i)) * scale +
             static_cast<float>(i) * gap;
    const std::vector<float> &p = borrowed ? *borrowed : prefix;
    return p[i] * scale + static_cast<float>(i) * gap;
  }
  [[nodiscard]] float total() const { return count ? offset_of(count) - gap : 0.f; }
  // Largest index whose offset is <= y, clamped to the list.
  [[nodiscard]] long index_at(float y) const {
    if (y <= 0.f)
      return 0;
    if (y >= total()) return static_cast<long>(count);
    if (is_uniform())
      return static_cast<long>(y / (uniform * scale + gap));
    size_t lo = 0, hi = count;
    while (lo < hi) {
      const size_t mid = lo + (hi - lo) / 2;
      if (offset_of(mid) <= y) lo = mid + 1;
      else hi = mid;
    }
    return static_cast<long>(lo ? lo - 1 : 0);
  }
  // Shortest row, for sizing the recycle pool against the worst case.
  [[nodiscard]] float shortest() const {
    if (is_uniform())
      return uniform * scale;
    float m = 0.f;
    for (size_t i = 0; i < count; i++)
      m = (i == 0) ? height_of(i) : std::min(m, height_of(i));
    return m > 0.f ? m : 1.f;
  }
};

// Bring the retained index to `count` rows: height_of is asked only for the
// dirty range, then the prefix is re-summed from the first stale entry.
template <typename HeightFn>
void refresh_virtual_index(HasVirtualListIndex &idx, size_t count,
                           HeightFn &&height_of) {
  const size_t old = idx.heights.size();
  if (count != old) {
    idx.heights.resize(count);
    idx.prefix.resize(count + 1);
    if (count > old)
      idx.invalidate(old, count);
  }
  // The clamps at use are what bound the range: dirty_from may sit at the
  // union identity, far past count, and clamping it in storage would
  // destroy the identity the next invalidate relies on.
  const size_t from = std::min(idx.dirty_from, count);
  const size_t to = std::min(idx.dirty_to, count);
  for (size_t i = from; i < to; i++) {
    const float h = height_of(i);
    idx.heights[i] = h > 0.f ? h : 1.f;
  }
  const size_t pfrom = std::min({idx.prefix_from, from, count});
  for (size_t i = pfrom; i < count; i++)
    idx.prefix[i + 1] = idx.prefix[i] + idx.heights[i];
  idx.dirty_from = HasVirtualListIndex::kDirtyEmpty;
  idx.dirty_to = 0;
  idx.prefix_from = count;
}
} // namespace detail


/// Windowed (virtualized) list. Builds only the rows that are on screen, while
/// the scroll bar still spans the whole list.
///
/// `render_row(index, row_parent)` builds one row's contents; every row is
/// `row_height` in the list's pixels() units, including Adaptive zoom. It builds the
/// thirty you can see.
///
/// ```cpp
/// virtual_list(ctx, mk(parent), items.size(), 26.f,
///   [&](size_t i, Entity &row) {
///     div(ctx, mk(row, 0), ComponentConfig{}.with_label(items[i]));
///   });
/// ```
template <typename RenderRow>
ElementResult virtual_list_impl(HasUIContext auto &ctx, EntityParent ep_pair,
                                detail::RowMetrics rows,
                                RenderRow &&render_row,
                                ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);
  const size_t count = rows.count;
  if (config.size.is_default)
    config.with_size(ComponentSize{percent(1.0f), percent(1.0f)});
  config.with_overflow(Overflow::Scroll, Axis::Y)
      .with_flex_direction(FlexDirection::Column)
      .with_justify_content(JustifyContent::FlexStart)
      .with_no_wrap();
  init_component(ctx, ep_pair, config, ComponentType::Div);
  auto &cmp = entity.template get<UIComponent>();
  auto &scroll = entity.template get<HasScrollView>();
  scroll.unbuilt_content_size.y = 0.f;
  if (count == 0) {
    scroll.scroll_offset.y = 0.f;
    scroll.scroll_target.y = 0.f;
    return {true, entity};
  }
  const auto mode = cmp.resolved_scaling_mode;
  rows.scale = mode == ScalingMode::Adaptive ? ctx.theme.ui_scale : 1.f;
  const float screen_h = detail::measure_screen_dim(Axis::Y);
  rows.gap = config.flex_gap.value > 0.f
      ? resolve_to_pixels(config.flex_gap, screen_h, mode, ctx.theme.ui_scale) : 0.f;
  if (rows.index) {
    rows.index->last_scale = rows.scale;
    rows.index->last_gap = rows.gap;
    // Rows inserted above the fold since the last build are measured by
    // now, so their height can move the scroll position in this same
    // build: the row under the reader stays under the reader.
    if (rows.index->pending_prepend > 0) {
      const float delta =
          rows.offset_of(std::min(rows.index->pending_prepend, count));
      scroll.scroll_offset.y += delta;
      scroll.scroll_target.y += delta;
      scroll.last_eased_offset.y += delta;
      rows.index->pending_prepend = 0;
    }
  }
  float parent_h = 0.f, parent_padding = 0.f;
  if (parent.template has<UIComponent>()) {
    const auto &parent_cmp = parent.template get<UIComponent>();
    parent_h = parent_cmp.computed[Axis::Y];
    parent_padding = parent_cmp.computed_padd[Axis::Y];
    const auto &desired = parent_cmp.desired[Axis::Y];
    if (desired.dim == Dim::Pixels || desired.dim == Dim::ScreenPercent)
      parent_h = resolve_to_pixels(desired, screen_h, parent_cmp.resolved_scaling_mode,
                                   ctx.theme.ui_scale);
  }
  const auto resolve_padding = [&](const Size &size) {
    if (size.dim == Dim::Percent) return size.value * parent_h;
    if (size.dim == Dim::Pixels || size.dim == Dim::ScreenPercent)
      return resolve_to_pixels(size, screen_h, mode, ctx.theme.ui_scale);
    return 0.f;
  };
  const float top_padding = resolve_padding(config.padding.top);
  const float bottom_padding = resolve_padding(config.padding.bottom);

  const long n = static_cast<long>(count);
  constexpr long OVERSCAN = 4;
  float lo = 0.f, hi = 0.f;

  if (entity.template has<HasScrollView>()) {
    auto &sv = entity.template get<HasScrollView>();
    float view_h = 0.f;
    if (config.size.y_axis.dim == Dim::Pixels) {
      view_h = resolve_to_pixels(config.size.y_axis, screen_h, mode, ctx.theme.ui_scale);
    } else if (sv.viewport_size.has_value()) {
      view_h = sv.viewport_size->y;
    } else if (config.size.y_axis.dim == Dim::Percent && parent_h > 0.f) {
      view_h = config.size.y_axis.value * std::max(0.f, parent_h - parent_padding);
    } else {
      // Nothing has measured this view yet. Ask what it was configured to be
      // rather than guessing a row count: a wrong guess on frame one used to
      // be permanent, and is still a frame of wasted work.
      view_h = measure_config(config, 0.f, std::max(0.f, parent_h - parent_padding)).size.y;
    }

    // Cover where the view is AND where it is easing to. Windowing on the
    // settled offset alone shows a strip of nothing for a frame on every
    // fling, which is never seen in development and gets reported as
    // "flickers when I scroll fast".
    const float max_offset = std::max(0.f, rows.total() + top_padding + bottom_padding - view_h);
    sv.scroll_offset.y = std::clamp(sv.scroll_offset.y, 0.f, max_offset);
    sv.scroll_target.y = std::clamp(sv.scroll_target.y, 0.f, max_offset);
    lo = std::min(sv.scroll_offset.y, sv.scroll_target.y) - top_padding;
    hi = std::max(sv.scroll_offset.y, sv.scroll_target.y) + view_h - top_padding;
    // Bounded, because dragging the scroll bar throws the target the length of
    // the list, and building every row in between is the whole list, which is
    // the cost this exists to avoid. Rows travelled past that fast cannot be
    // read anyway, so favour the end being moved toward.
    const float max_span = std::max(view_h * 3.f, 1.f);
    if (hi - lo > max_span) {
      if (sv.scroll_target.y >= sv.scroll_offset.y)
        lo = hi - max_span;
      else
        hi = lo + max_span;
    }
  }

  size_t first = 0, last = static_cast<size_t>(std::min<long>(n, 1));
  if (hi > lo) {
    long f = rows.index_at(lo) - OVERSCAN;
    long l = rows.index_at(hi) + OVERSCAN + 1;
    first = static_cast<size_t>(std::clamp<long>(f, 0, n));
    last = static_cast<size_t>(std::clamp<long>(l, 0, n));
  }
  const size_t window = (last > first) ? (last - first) : 0;

  // How many row entities get recycled. Derived from the viewport rather than
  // from this frame's window, and rounded up to a block, because a modulus
  // that changes moves every recycled row onto a different item at once: a
  // press is stranded and the list cannot settle. Rounding keeps it still
  // while the window breathes by a row.
  size_t capacity = window + 2 * (size_t)OVERSCAN + 8;
  if (entity.template has<HasScrollView>()) {
    auto &sv = entity.template get<HasScrollView>();
    if (sv.viewport_size.has_value()) {
      // Has to cover the widest window the span bound above can ask for, or
      // two items would land on the same row entity.
      const float widest = (3.f * sv.viewport_size->y) / rows.shortest();
      capacity = std::max(capacity, static_cast<size_t>(widest) + 2 * (size_t)OVERSCAN + 8);
    }
    constexpr size_t BLOCK = 32;
    capacity = ((capacity + BLOCK - 1) / BLOCK) * BLOCK;
    // Rows above and below that were not built still have to count, or the bar
    // would size itself to the window instead of to the list.
    // Only the rows BELOW: the leading spacer is a real child, so the rows
    // above are already in the measured total. Counting them here too made
    // content_size grow as the list scrolled, and a scrollbar drag chased it.
    sv.unbuilt_content_size.y = last < count
        ? rows.total() - rows.offset_of(last) + rows.gap : 0.f;
  }

  // The rows that were skipped above still have to occupy their space, because
  // the scroll offset is applied when drawing rather than when laying out: with
  // nothing in front of it the first built row lands at the top of the content
  // and is then drawn off screen.
  if (first > 0) {
    div(ctx, mk(entity, 0),
        ComponentConfig{}
            .with_size(ComponentSize{
                percent(1.0f),
                pixels((rows.offset_of(first) - rows.gap) / rows.scale)})
            .with_scaling_mode(mode)
            .with_skip_grid_snap()
            .with_transparent_bg()
            .with_debug_name("vlist_skipped_above"));
  }

  for (size_t i = first; i < last; ++i) {
    bool now_a_different_item = false;
    auto row = div(ctx,
                   detail::mk_keyed(entity, static_cast<EntityID>(i % capacity),
                                    i + 1, &now_a_different_item),
                   ComponentConfig{}
                       .with_size(
                           ComponentSize{percent(1.0f), pixels(rows.height_of(i) / rows.scale)})
                       .with_scaling_mode(mode)
                       .with_skip_grid_snap()
                       .with_transparent_bg()
                       .with_debug_name("vlist_row"));
    // This entity was showing another item a moment ago, and everything the
    // library hangs off an entity id belongs to the position rather than to
    // the item. Left alone, a press that began on the row that used to be here
    // finishes on the one that is here now.
    if (now_a_different_item) {
      Entity &re = row.ent();
      if (re.template has<HasClickListener>())
        re.template get<HasClickListener>().down = false;
      if (ctx.is_active(re.id))
        ctx.set_active(ctx.ROOT);
      if (ctx.is_hot(re.id))
        ctx.set_hot(ctx.ROOT);
    }
    render_row(i, row.ent());
  }
  return {true, entity};
}

/// Every row the same height.
template <typename RenderRow>
ElementResult virtual_list(HasUIContext auto &ctx, EntityParent ep_pair,
                           size_t count, float row_height,
                           RenderRow &&render_row,
                           ComponentConfig config = ComponentConfig()) {
  return virtual_list_impl(ctx, ep_pair,
                           detail::RowMetrics::uniform_rows(count, row_height),
                           std::forward<RenderRow>(render_row), config);
}

/// Rows of differing heights, retained: the prefix-height index lives on
/// the list entity (`HasVirtualListIndex`), so `height_of(index)` is asked
/// once per row until that row is invalidated or new. The window is found
/// by binary search over the retained total.
///
/// ```cpp
/// virtual_list(ctx, mk(parent), msgs.size(),
///   [&](size_t i) { return msgs[i].wrapped_height; },
///   [&](size_t i, Entity &row) { ... });
/// ```
template <typename HeightFn, typename RenderRow>
ElementResult virtual_list(HasUIContext auto &ctx, EntityParent ep_pair,
                           size_t count, HeightFn &&height_of,
                           RenderRow &&render_row,
                           ComponentConfig config = ComponentConfig())
  requires std::invocable<HeightFn, size_t>
{
  auto [entity, parent] = deref(ep_pair);
  auto &index = entity.template addComponentIfMissing<HasVirtualListIndex>();
  detail::refresh_virtual_index(index, count,
                                std::forward<HeightFn>(height_of));
  detail::RowMetrics rows;
  rows.count = count;
  rows.borrowed = &index.prefix;
  rows.index = &index;
  return virtual_list_impl(ctx, ep_pair, rows,
                           std::forward<RenderRow>(render_row), config);
}

/// Mark rows [first, last) of a measured virtual_list as changed: the next
/// build re-asks height_of for exactly those rows.
inline void invalidate_virtual_rows(Entity &list, size_t first, size_t last) {
  if (list.template has<HasVirtualListIndex>())
    list.template get<HasVirtualListIndex>().invalidate(first, last);
}

/// Insert `count` new rows at the front of a measured virtual_list (load
/// older, chat style). Call before the next build with the grown count:
/// the build measures the new rows and adds their height to the scroll
/// position in the same pass, so the row under the reader does not move.
inline void prepend_virtual_rows(Entity &list, size_t count) {
  if (count == 0 || !list.template has<HasVirtualListIndex>())
    return;
  auto &idx = list.template get<HasVirtualListIndex>();
  idx.heights.insert(idx.heights.begin(), count, 0.f);
  idx.prefix.insert(idx.prefix.begin(), count, 0.f);
  // Shift the pending range by the insertion, saturating at the union
  // identity so an empty range stays empty.
  const size_t cap = HasVirtualListIndex::kDirtyEmpty - count;
  idx.dirty_from = std::min(idx.dirty_from, cap) + count;
  idx.dirty_to = std::min(idx.dirty_to, cap) + count;
  idx.invalidate(0, count);
  idx.prefix_from = 0;
  idx.pending_prepend += count;
}

/// Where row `index` starts inside the content, in pixels, as of the last
/// build of a measured virtual_list. 0 when the list has no retained index.
[[nodiscard]] inline float virtual_row_offset(const Entity &list,
                                              size_t index) {
  if (!list.template has<HasVirtualListIndex>())
    return 0.f;
  const auto &idx = list.template get<HasVirtualListIndex>();
  if (index >= idx.prefix.size())
    return 0.f;
  return idx.prefix[index] * idx.last_scale +
         static_cast<float>(index) * idx.last_gap;
}

/// Total height of a measured virtual_list's rows, in pixels, as of its
/// last build.
[[nodiscard]] inline float virtual_rows_total(const Entity &list) {
  if (!list.template has<HasVirtualListIndex>())
    return 0.f;
  const auto &idx = list.template get<HasVirtualListIndex>();
  if (idx.prefix.empty())
    return 0.f;
  const size_t count = idx.prefix.size() - 1;
  return idx.prefix[count] * idx.last_scale +
         static_cast<float>(count ? count - 1 : 0) * idx.last_gap;
}


} // namespace imm

} // namespace ui

} // namespace afterhours
