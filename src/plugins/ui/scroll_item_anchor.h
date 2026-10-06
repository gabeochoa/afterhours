#pragma once

// Logical scroll anchors: keep an ITEM at a place in the viewport across
// replaced entities, pages, zoom and resizes.
//
// HasScrollView::anchor_scroll pins a rendered child entity, which is right
// for ordinary lists and wrong for a virtualized one, where that entity is
// recycled for a different row. Here the caller owns the identity: an opaque
// key it can resolve to the item's current geometry once the content that
// key belongs to is laid out. Nothing retries on a frame count; a request
// stays pending until it applies, turns out unavailable, or the user scrolls.
//
//   // Remember, while the item is on screen:
//   auto anchor = capture_scroll_anchor(scroll, "msg:4812", {top, height}, .3f);
//   anchor.generation = doc_generation;
//   ...
//   request_scroll_anchor(view, anchor);
//   // Each frame, from a system after the UI post-layout systems:
//   resolve_scroll_anchor(view, ready_generation, [&](std::string_view key)
//       -> std::optional<ItemGeometry> { ... });

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "../../ecs.h"
#include "components.h"

namespace afterhours {
namespace ui {

/// Where an item is, in the scroll view's content pixels (the space
/// scroll_offset counts in).
struct ItemGeometry {
  float top = 0.f;
  float height = 0.f;
};

struct ScrollItemAnchor {
  std::string key;
  // The point within the item, as a fraction of its height, so it survives
  // zoom and reflow that change the height.
  float within = 0.f;
  // Where that point sits in the unoccluded band of the viewport: 0 top, 1
  // bottom.
  float fraction = 0.f;
  // Pixels of viewport covered by a sticky header or footer.
  float inset_top = 0.f;
  float inset_bottom = 0.f;
  // Which content and layout the key refers to. The request waits until the
  // host reports this generation ready.
  std::uint64_t generation = 0;
};

enum struct AnchorStatus { Pending, Applied, Unavailable, Cancelled };

struct HasScrollItemAnchor : BaseComponent {
  ScrollItemAnchor anchor;
  AnchorStatus status = AnchorStatus::Pending;
  std::uint64_t scroll_serial = 0;
};

namespace detail {
inline float anchor_band(const ScrollItemAnchor &a, float viewport) {
  return std::max(0.f, viewport - a.inset_top - a.inset_bottom);
}
} // namespace detail

/// The anchor that would put `item` back where it is now. `fraction` picks
/// which point of the viewport band to remember -- usually where the reader
/// is looking, near the top.
inline ScrollItemAnchor capture_scroll_anchor(const HasScrollView &scroll,
                                              std::string key, ItemGeometry item,
                                              float fraction, float inset_top = 0.f,
                                              float inset_bottom = 0.f) {
  ScrollItemAnchor a{std::move(key), 0.f, fraction, inset_top, inset_bottom, 0};
  const float y = scroll.scroll_offset.y + inset_top +
                  fraction * detail::anchor_band(a, scroll.viewport_or_zero().y);
  a.within = item.height > 0.f ? (y - item.top) / item.height : 0.f;
  return a;
}

/// Replaces any pending request. User scrolling from here on cancels it.
inline void request_scroll_anchor(Entity &view, ScrollItemAnchor anchor) {
  auto &state = view.addComponentIfMissing<HasScrollItemAnchor>();
  state.anchor = std::move(anchor);
  state.status = AnchorStatus::Pending;
  state.scroll_serial = view.get<HasScrollView>().user_scroll_serial;
}

/// Applies a pending request once `ready_generation` matches its generation
/// and the viewport is measured. `resolve(key)` returns the item's current
/// geometry, or nullopt when the ready content no longer has it. A
/// generation past the requested one is Unavailable: the content the key
/// named was replaced. Returns the request's status (Applied etc. stay put
/// until the next request); a view never asked returns Unavailable.
template <typename Resolve>
AnchorStatus resolve_scroll_anchor(Entity &view, std::uint64_t ready_generation,
                                   Resolve &&resolve) {
  if (!view.has<HasScrollItemAnchor>() || !view.has<HasScrollView>())
    return AnchorStatus::Unavailable;
  auto &state = view.get<HasScrollItemAnchor>();
  if (state.status != AnchorStatus::Pending)
    return state.status;
  auto &scroll = view.get<HasScrollView>();
  if (scroll.user_scroll_serial != state.scroll_serial)
    return state.status = AnchorStatus::Cancelled;
  if (ready_generation < state.anchor.generation || !scroll.viewport_size)
    return state.status;
  if (ready_generation > state.anchor.generation)
    return state.status = AnchorStatus::Unavailable;
  const std::optional<ItemGeometry> item = resolve(std::string_view(state.anchor.key));
  if (!item)
    return state.status = AnchorStatus::Unavailable;
  const auto &a = state.anchor;
  const float y = item->top + a.within * item->height - a.inset_top -
                  a.fraction * detail::anchor_band(a, scroll.viewport_size->y);
  scroll.jump_to_y(std::clamp(y, 0.f, scroll.bottom_offset()));
  // The item is the anchor now; a pinned child entity would fight it.
  scroll.anchor_child = -1;
  return state.status = AnchorStatus::Applied;
}

} // namespace ui
} // namespace afterhours
