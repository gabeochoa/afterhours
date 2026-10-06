#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <cmath>
#include <unordered_map>
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
  // Cached, because scanning every row for it each frame is O(n) per build.
  float minimum = 1.f;

  static float sane(float h) { return std::isfinite(h) && h > 0.f ? h : 1.f; }

  static RowMetrics uniform_rows(size_t n, float h) {
    RowMetrics m;
    m.uniform = m.minimum = sane(h);
    m.count = n;
    return m;
  }

  // Precomputed heights, owned by the metrics rather than a list entity.
  template <typename HeightFn>
  static RowMetrics measured_rows(size_t n, HeightFn &&height_of) {
    RowMetrics m;
    m.count = n;
    m.prefix.resize(n + 1, 0.f);
    for (size_t i = 0; i < n; i++) {
      const float h = sane(height_of(i));
      m.minimum = i == 0 ? h : std::min(m.minimum, h);
      m.prefix[i + 1] = m.prefix[i] + h;
    }
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
  [[nodiscard]] float shortest() const { return minimum * scale; }
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
  for (size_t i = from; i < to; i++)
    idx.heights[i] = RowMetrics::sane(height_of(i));
  const size_t pfrom = std::min({idx.prefix_from, from, count});
  // Only a change can move the shortest row, so the scan rides on the re-sum.
  if (pfrom < count || count != old) {
    idx.minimum = 1.f;
    for (size_t i = 0; i < count; i++)
      idx.minimum = i == 0 ? idx.heights[i] : std::min(idx.minimum, idx.heights[i]);
  }
  for (size_t i = pfrom; i < count; i++)
    idx.prefix[i + 1] = idx.prefix[i] + idx.heights[i];
  idx.dirty_from = HasVirtualListIndex::kDirtyEmpty;
  idx.dirty_to = 0;
  idx.prefix_from = count;
}
} // namespace detail

using VirtualListMetrics = detail::RowMetrics;

/// How far past the viewport a virtual_list builds. Rows are the old fixed
/// four; pixels add a zoom-proportional margin; maximum_viewports bounds how
/// far toward a distant easing target (a scrollbar drag) the window reaches.
struct VirtualListOptions {
  size_t overscan_rows = 4;
  float overscan_pixels = 0.f;
  float maximum_viewports = 3.f;
};

/// Rows [first, last) to build, and [visible_first, visible_last) actually
/// inside the viewport at `offset`.
struct VirtualListWindow {
  size_t first = 0, last = 0;
  size_t visible_first = 0, visible_last = 0;
};

/// The window for a scroll position. Offsets are in the metrics' pixels,
/// measured from the first row. The span reaches toward `target` but is
/// bounded around `offset`, so a far target never evicts the rows on screen.
inline VirtualListWindow virtual_list_window(const VirtualListMetrics &rows,
                                             float offset, float target,
                                             float viewport,
                                             VirtualListOptions options = {}) {
  if (rows.count == 0)
    return {};
  if (!std::isfinite(viewport) || viewport <= 0.f)
    return {0, 1, 0, 1};
  const float end = std::max(0.f, rows.total() - viewport);
  offset = std::clamp(std::isfinite(offset) ? offset : 0.f, 0.f, end);
  target = std::clamp(std::isfinite(target) ? target : offset, 0.f, end);
  const float viewports = std::clamp(
      std::isfinite(options.maximum_viewports) ? options.maximum_viewports : 3.f,
      1.f, 8.f);
  target = std::clamp(target, offset - viewport * (viewports - 1.f),
                      offset + viewport * (viewports - 1.f));
  const float buffer = std::clamp(
      std::isfinite(options.overscan_pixels) ? options.overscan_pixels : 0.f,
      0.f, viewport * 2.f);
  const auto index = [&](float y) {
    return std::min(rows.count, static_cast<size_t>(std::max(0L, rows.index_at(y))));
  };
  const size_t first = index(std::min(offset, target) - buffer);
  const size_t last = std::min(
      rows.count, index(std::max(offset, target) + viewport + buffer) + 1);
  const size_t overscan = std::min(options.overscan_rows, size_t{256});
  return {first > overscan ? first - overscan : 0,
          last + std::min(overscan, rows.count - last), index(offset),
          std::min(rows.count, index(offset + viewport) + 1)};
}

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
                                ComponentConfig config = ComponentConfig(),
                                VirtualListOptions options = {}) {
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
    rows.minimum = rows.index->minimum;
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

  float offset = 0.f, target = 0.f, view_h = 0.f;
  if (entity.template has<HasScrollView>()) {
    auto &sv = entity.template get<HasScrollView>();
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
    const bool pinned = rows.index && rows.index->pin_row < count;
    if (pinned) {
      sv.scroll_offset.y = sv.scroll_target.y = top_padding +
          rows.offset_of(rows.index->pin_row) + rows.index->pin_within;
      // The pinned row is the caller's anchor; a pinned child entity is not.
      sv.anchor_child = -1;
    }
    const float max_offset = std::max(0.f, rows.total() + top_padding + bottom_padding - view_h);
    sv.scroll_offset.y = std::clamp(sv.scroll_offset.y, 0.f, max_offset);
    sv.scroll_target.y = std::clamp(sv.scroll_target.y, 0.f, max_offset);
    if (pinned)
      sv.last_eased_offset.y = sv.scroll_offset.y;
    if (rows.index) {
      rows.index->pin_row = HasVirtualListIndex::kNoPin;
      // Signed, so a view resting inside the top padding pins back there.
      const float y = sv.scroll_offset.y - top_padding;
      const size_t top = std::min(count - 1, static_cast<size_t>(rows.index_at(y)));
      rows.index->top_row = top;
      rows.index->top_within = y - rows.offset_of(top);
    }
    // Cover where the view is AND where it is easing to, or a fling shows a
    // strip of nothing for a frame. virtual_list_window bounds the reach
    // toward the target around the actual viewport, so a scrollbar drag
    // cannot ask for the whole list or drop the rows on screen.
    offset = sv.scroll_offset.y - top_padding;
    target = sv.scroll_target.y - top_padding;
  }
  const auto range = virtual_list_window(rows, offset, target, view_h, options);
  const size_t first = range.first, last = range.last;
  const size_t window = last - first;
  const size_t OVERSCAN = std::min(options.overscan_rows, size_t{256});
  const float viewports = std::clamp(
      std::isfinite(options.maximum_viewports) ? options.maximum_viewports : 3.f,
      1.f, 8.f);

  // How many row entities get recycled. Derived from the viewport rather than
  // from this frame's window, and rounded up to a block, because a modulus
  // that changes moves every recycled row onto a different item at once: a
  // press is stranded and the list cannot settle. Rounding keeps it still
  // while the window breathes by a row.
  size_t capacity = window + 2 * OVERSCAN + 8;
  if (entity.template has<HasScrollView>()) {
    auto &sv = entity.template get<HasScrollView>();
    if (sv.viewport_size.has_value()) {
      // Has to cover the widest window virtual_list_window can ask for --
      // the bounded span plus up to two viewports of pixel overscan each
      // side -- or two items would land on the same row entity.
      const float widest = ((viewports + 4.f) * sv.viewport_size->y) / rows.shortest();
      capacity = std::max(capacity, static_cast<size_t>(widest) + 2 * OVERSCAN + 8);
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
                           ComponentConfig config = ComponentConfig(),
                           VirtualListOptions options = {}) {
  return virtual_list_impl(ctx, ep_pair,
                           detail::RowMetrics::uniform_rows(count, row_height),
                           std::forward<RenderRow>(render_row), config, options);
}

/// Rows the caller has already measured (VirtualListMetrics::measured_rows),
/// so one set of extents drives both the window and any other geometry the
/// caller needs, instead of a second running total kept beside the list.
template <typename RenderRow>
ElementResult virtual_list(HasUIContext auto &ctx, EntityParent ep_pair,
                           const VirtualListMetrics &rows,
                           RenderRow &&render_row,
                           ComponentConfig config = ComponentConfig(),
                           VirtualListOptions options = {}) {
  return virtual_list_impl(ctx, ep_pair, rows,
                           std::forward<RenderRow>(render_row), config, options);
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
                           ComponentConfig config = ComponentConfig(),
                           VirtualListOptions options = {})
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
                           std::forward<RenderRow>(render_row), config, options);
}

/// A measured virtual_list whose rows are items with stable keys. When the
/// sequence changes (bump `revision`), the item at the top of the viewport
/// stays there -- or, if it was removed, the nearest survivor in the old
/// order takes its place -- and focus inside a row follows its item to
/// whichever pooled entity now shows it. An empty sequence keeps the old
/// anchor for the next non-empty one, so clearing a filter returns to where
/// the reader was.
///
/// `key_of(index)` is asked for every row only when `revision` changes.
/// ponytail: a change re-measures every row; remap heights by key if
/// height_of is too slow to run over the whole list on each update.
template <typename KeyFn, typename HeightFn, typename RenderRow>
ElementResult keyed_virtual_list(HasUIContext auto &ctx, EntityParent ep_pair,
                                 size_t count, std::uint64_t revision,
                                 KeyFn &&key_of, HeightFn &&height_of,
                                 RenderRow &&render_row,
                                 ComponentConfig config = ComponentConfig(),
                                 VirtualListOptions options = {}) {
  auto [entity, parent] = deref(ep_pair);
  auto &keyed = entity.template addComponentIfMissing<HasVirtualListKeys>();
  auto &index = entity.template addComponentIfMissing<HasVirtualListIndex>();
  // Focus moved since the last build (a click, Tab): find which item it
  // landed in from what that build showed, before positions change.
  const auto sample_focus = [&] {
    if (ctx.focus_id == keyed.focus_entity)
      return;
    keyed.focus_entity = ctx.focus_id;
    keyed.focus_key.clear();
    for (const auto &[key, subtree] : keyed.shown) {
      const auto at = std::find(subtree.begin(), subtree.end(), ctx.focus_id);
      if (at == subtree.end())
        continue;
      keyed.focus_key = key;
      keyed.focus_index = static_cast<size_t>(at - subtree.begin());
    }
  };
  sample_focus();
  if (!keyed.seen || keyed.revision != revision) {
    std::vector<std::string> next;
    next.reserve(count);
    for (size_t i = 0; i < count; i++)
      next.emplace_back(key_of(i));
    if (keyed.seen && count > 0 && !keyed.keys.empty()) {
      std::unordered_map<std::string_view, size_t> where;
      where.reserve(count);
      for (size_t i = 0; i < count; i++)
        where.emplace(next[i], i);
      const auto &old = keyed.keys;
      const size_t top = std::min(index.top_row, old.size() - 1);
      // Nearest survivor in the old order, the row below first: removing
      // the top row should bring up the one that followed it.
      for (size_t d = 0; d < old.size(); d++) {
        auto hit = top + d < old.size() ? where.find(old[top + d]) : where.end();
        if (hit == where.end() && d <= top)
          hit = where.find(old[top - d]);
        if (hit == where.end())
          continue;
        index.pin_row = hit->second;
        index.pin_within = d == 0 ? index.top_within : 0.f;
        break;
      }
    }
    // Heights were stored by position, and positions just moved.
    index.invalidate(0, count);
    keyed.revision = revision;
    keyed.seen = true;
    if (count > 0)
      keyed.keys = std::move(next);
  }

  std::vector<std::pair<size_t, EntityID>> built;
  detail::refresh_virtual_index(index, count, std::forward<HeightFn>(height_of));
  detail::RowMetrics rows;
  rows.count = count;
  rows.borrowed = &index.prefix;
  rows.index = &index;
  auto result = virtual_list_impl(
      ctx, ep_pair, rows,
      [&](size_t i, Entity &row) {
        built.emplace_back(i, row.id);
        render_row(i, row);
      },
      config, options);
  keyed.shown.clear();
  if (count == 0 || keyed.keys.size() != count)
    return result;
  for (const auto &[i, row] : built) {
    std::vector<EntityID> subtree;
    const std::function<void(EntityID)> walk = [&](EntityID id) {
      subtree.push_back(id);
      for (const EntityID child : UICollectionHolder::getEntityForIDEnforce(id)
                                      .template get<UIComponent>().children)
        walk(child);
    };
    walk(row);
    keyed.shown.emplace_back(keyed.keys[i], std::move(subtree));
  }
  // The focused item is now drawn by a different pooled row: focus the entity
  // in the same place in that row. Off screen, the item is remembered until
  // it is built again.
  if (!keyed.focus_key.empty() && ctx.focus_id == keyed.focus_entity) {
    for (const auto &[key, subtree] : keyed.shown) {
      if (key != keyed.focus_key || keyed.focus_index >= subtree.size())
        continue;
      if (subtree[keyed.focus_index] != ctx.focus_id)
        ctx.set_focus(subtree[keyed.focus_index], ctx.focus_modality);
      keyed.focus_entity = ctx.focus_id;
    }
  }
  sample_focus();
  return result;
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
