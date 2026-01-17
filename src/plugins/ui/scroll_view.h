// Scrollable Container Components and Systems for Afterhours
// Gap 08: Scrollable Containers
//
// Usage:
//   entity.add<HasScrollView>({.horizontal_enabled = false, .vertical_enabled = true});
//   // In render loop:
//   ScrollViewRenderSystem::apply_scroll_clip(entity, widget);
//   // ... render children ...
//   ScrollViewRenderSystem::end_scroll_clip();

#pragma once

#include <algorithm>
#include <cmath>

#include "../../ecs.h"
#include "../color.h"
#include "../graphics_backend.h"
#include "../input_provider.h"
#include "ui_core_components.h"

namespace afterhours {
namespace ui {

//=============================================================================
// COMPONENTS
//=============================================================================

/// Scroll view state and configuration
struct HasScrollView : BaseComponent {
  // Current scroll position (pixels from origin)
  float scroll_offset_x = 0.f;
  float scroll_offset_y = 0.f;

  // Size of the scrollable content (may be larger than viewport)
  float content_width = 0.f;
  float content_height = 0.f;

  // Visible viewport size (set by layout system)
  float viewport_width = 0.f;
  float viewport_height = 0.f;

  // Configuration
  bool horizontal_enabled = false;
  bool vertical_enabled = true;
  float scroll_speed = 40.f;       // Pixels per scroll wheel notch
  float keyboard_scroll_speed = 100.f; // Pixels per arrow key press
  bool smooth_scrolling = false;   // Enable smooth scroll animation
  float smooth_factor = 0.15f;     // Lerp factor for smooth scroll

  // Smooth scrolling target (when smooth_scrolling enabled)
  float target_scroll_x = 0.f;
  float target_scroll_y = 0.f;

  // === Computed Properties ===

  float max_scroll_x() const {
    return std::max(0.f, content_width - viewport_width);
  }

  float max_scroll_y() const {
    return std::max(0.f, content_height - viewport_height);
  }

  float scroll_ratio_x() const {
    float max = max_scroll_x();
    return max > 0.f ? scroll_offset_x / max : 0.f;
  }

  float scroll_ratio_y() const {
    float max = max_scroll_y();
    return max > 0.f ? scroll_offset_y / max : 0.f;
  }

  bool can_scroll_left() const {
    return horizontal_enabled && scroll_offset_x > 0.f;
  }

  bool can_scroll_right() const {
    return horizontal_enabled && scroll_offset_x < max_scroll_x();
  }

  bool can_scroll_up() const {
    return vertical_enabled && scroll_offset_y > 0.f;
  }

  bool can_scroll_down() const {
    return vertical_enabled && scroll_offset_y < max_scroll_y();
  }

  bool needs_horizontal_scrollbar() const {
    return horizontal_enabled && content_width > viewport_width;
  }

  bool needs_vertical_scrollbar() const {
    return vertical_enabled && content_height > viewport_height;
  }

  // === Methods ===

  void clamp() {
    scroll_offset_x = std::clamp(scroll_offset_x, 0.f, max_scroll_x());
    scroll_offset_y = std::clamp(scroll_offset_y, 0.f, max_scroll_y());
    if (smooth_scrolling) {
      target_scroll_x = std::clamp(target_scroll_x, 0.f, max_scroll_x());
      target_scroll_y = std::clamp(target_scroll_y, 0.f, max_scroll_y());
    }
  }

  void scroll_to(float x, float y) {
    if (smooth_scrolling) {
      target_scroll_x = x;
      target_scroll_y = y;
    } else {
      scroll_offset_x = x;
      scroll_offset_y = y;
    }
    clamp();
  }

  void scroll_by(float dx, float dy) {
    if (smooth_scrolling) {
      target_scroll_x += dx;
      target_scroll_y += dy;
    } else {
      scroll_offset_x += dx;
      scroll_offset_y += dy;
    }
    clamp();
  }

  /// Scroll to make a rectangle visible within the viewport
  void scroll_to_visible(float rect_x, float rect_y, float rect_w,
                         float rect_h, float margin = 0.f) {
    float new_x = scroll_offset_x;
    float new_y = scroll_offset_y;

    // Horizontal visibility
    if (horizontal_enabled) {
      if (rect_x < scroll_offset_x + margin) {
        new_x = rect_x - margin;
      } else if (rect_x + rect_w > scroll_offset_x + viewport_width - margin) {
        new_x = rect_x + rect_w - viewport_width + margin;
      }
    }

    // Vertical visibility
    if (vertical_enabled) {
      if (rect_y < scroll_offset_y + margin) {
        new_y = rect_y - margin;
      } else if (rect_y + rect_h > scroll_offset_y + viewport_height - margin) {
        new_y = rect_y + rect_h - viewport_height + margin;
      }
    }

    scroll_to(new_x, new_y);
  }

  /// Update smooth scrolling (call once per frame)
  void update_smooth_scroll() {
    if (!smooth_scrolling) return;
    scroll_offset_x +=
        (target_scroll_x - scroll_offset_x) * smooth_factor;
    scroll_offset_y +=
        (target_scroll_y - scroll_offset_y) * smooth_factor;
    // Snap when close enough
    if (std::abs(target_scroll_x - scroll_offset_x) < 0.5f) {
      scroll_offset_x = target_scroll_x;
    }
    if (std::abs(target_scroll_y - scroll_offset_y) < 0.5f) {
      scroll_offset_y = target_scroll_y;
    }
  }
};

/// Scrollbar visual style configuration
struct HasScrollbarStyle : BaseComponent {
  float width = 12.f;
  float min_thumb_size = 20.f;
  float corner_radius = 0.f;  // 0 = square, > 0 = rounded
  bool auto_hide = true;      // Hide when not needed
  float hide_delay = 1.5f;    // Seconds before auto-hide

  // Colors
  Color track_color{200, 200, 200, 100};
  Color thumb_color{150, 150, 150, 200};
  Color thumb_hover_color{120, 120, 120, 230};
  Color thumb_active_color{100, 100, 100, 255};

  // State (managed by system)
  bool is_dragging = false;
  bool is_hovering = false;
  float time_since_scroll = 0.f;
};

/// Tag component: entity clips its children to its bounds
struct ClipsChildren : BaseComponent {};

/// Scroll container focus tracking
struct HasScrollFocus : BaseComponent {
  bool has_focus = false;
};

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

namespace scroll {

/// Check if mouse position is inside a scrolled element's visible bounds
inline bool is_mouse_inside_viewport(float mouse_x, float mouse_y,
                                     float viewport_x, float viewport_y,
                                     float viewport_w, float viewport_h) {
  return mouse_x >= viewport_x && mouse_x < viewport_x + viewport_w &&
         mouse_y >= viewport_y && mouse_y < viewport_y + viewport_h;
}

/// Transform a point from viewport coordinates to content coordinates
inline void viewport_to_content(float& x, float& y,
                                const HasScrollView& scroll_view) {
  x += scroll_view.scroll_offset_x;
  y += scroll_view.scroll_offset_y;
}

/// Transform a point from content coordinates to viewport coordinates
inline void content_to_viewport(float& x, float& y,
                                const HasScrollView& scroll_view) {
  x -= scroll_view.scroll_offset_x;
  y -= scroll_view.scroll_offset_y;
}

/// Calculate scrollbar thumb position and size
struct ScrollbarMetrics {
  float track_pos;   // Start of track
  float track_size;  // Length of track
  float thumb_pos;   // Start of thumb within track
  float thumb_size;  // Length of thumb
};

inline ScrollbarMetrics calculate_scrollbar(float viewport_size,
                                            float content_size,
                                            float scroll_offset,
                                            float min_thumb_size = 20.f) {
  ScrollbarMetrics m{};
  m.track_pos = 0;
  m.track_size = viewport_size;

  if (content_size <= viewport_size) {
    m.thumb_pos = 0;
    m.thumb_size = viewport_size;
    return m;
  }

  // Thumb size proportional to visible ratio
  float ratio = viewport_size / content_size;
  m.thumb_size = std::max(min_thumb_size, m.track_size * ratio);

  // Thumb position
  float scrollable = content_size - viewport_size;
  float track_scrollable = m.track_size - m.thumb_size;
  float scroll_ratio = scrollable > 0 ? scroll_offset / scrollable : 0;
  m.thumb_pos = track_scrollable * scroll_ratio;

  return m;
}

/// Convert scrollbar drag position to scroll offset
inline float scrollbar_drag_to_offset(float drag_pos, float track_size,
                                      float thumb_size, float content_size,
                                      float viewport_size) {
  float track_scrollable = track_size - thumb_size;
  if (track_scrollable <= 0) return 0;

  float ratio = drag_pos / track_scrollable;
  float scrollable = content_size - viewport_size;
  return ratio * scrollable;
}

/// Find the nearest scroll parent by walking up the entity tree
/// Returns nullptr if no scroll parent found
inline const Entity* find_scroll_parent(const Entity& entity) {
  if (!entity.has<UIComponent>()) return nullptr;
  
  EntityID parent_id = entity.get<UIComponent>().parent;
  if (parent_id < 0) return nullptr;
  
  auto parent_opt = EntityHelper::getEntityForID(parent_id);
  if (!parent_opt.has_value()) return nullptr;
  
  const Entity& parent = parent_opt.asE();
  if (parent.has<HasScrollView>()) {
    return &parent;
  }
  
  // Recursively walk up the tree
  return find_scroll_parent(parent);
}

/// Find scroll parent and return its scroll view component
/// Returns nullptr if no scroll parent found
inline HasScrollView* find_scroll_view(Entity& entity) {
  if (!entity.has<UIComponent>()) return nullptr;
  
  EntityID parent_id = entity.get<UIComponent>().parent;
  if (parent_id < 0) return nullptr;
  
  auto parent_opt = EntityHelper::getEntityForID(parent_id);
  if (!parent_opt.has_value()) return nullptr;
  
  Entity& parent = parent_opt.asE();
  if (parent.has<HasScrollView>()) {
    return &parent.get<HasScrollView>();
  }
  
  return find_scroll_view(parent);
}

} // namespace scroll

//=============================================================================
// RENDER HELPERS
//=============================================================================

namespace scroll_render {

/// Begin scissor clipping for scroll container
inline void begin_clip(float x, float y, float width, float height) {
  graphics::begin_scissor_mode(static_cast<int>(x), static_cast<int>(y),
                               static_cast<int>(width),
                               static_cast<int>(height));
}

/// End scissor clipping
inline void end_clip() { graphics::end_scissor_mode(); }

/// Apply scroll translation for content rendering
inline void begin_scroll_transform(const HasScrollView& sv) {
  graphics::push_matrix();
  graphics::translate(-sv.scroll_offset_x, -sv.scroll_offset_y);
}

/// End scroll translation
inline void end_scroll_transform() { graphics::pop_matrix(); }

/// Draw vertical scrollbar
inline void draw_vertical_scrollbar(float viewport_x, float viewport_y,
                                    float viewport_w, float viewport_h,
                                    const HasScrollView& sv,
                                    const HasScrollbarStyle& style) {
  if (!sv.needs_vertical_scrollbar()) {
    if (style.auto_hide) return;
  }

  auto metrics = scroll::calculate_scrollbar(
      sv.viewport_height, sv.content_height, sv.scroll_offset_y,
      style.min_thumb_size);

  float bar_x = viewport_x + viewport_w - style.width;

  // Draw track
  graphics::Rect track_rect{bar_x, viewport_y, style.width, viewport_h};
  graphics::draw_rectangle(track_rect, style.track_color);

  // Draw thumb
  Color thumb_color = style.thumb_color;
  if (style.is_dragging) {
    thumb_color = style.thumb_active_color;
  } else if (style.is_hovering) {
    thumb_color = style.thumb_hover_color;
  }

  graphics::Rect thumb_rect{bar_x, viewport_y + metrics.thumb_pos,
                            style.width, metrics.thumb_size};
  graphics::draw_rectangle(thumb_rect, thumb_color);
}

/// Draw horizontal scrollbar
inline void draw_horizontal_scrollbar(float viewport_x, float viewport_y,
                                      float viewport_w, float viewport_h,
                                      const HasScrollView& sv,
                                      const HasScrollbarStyle& style) {
  if (!sv.needs_horizontal_scrollbar()) {
    if (style.auto_hide) return;
  }

  auto metrics = scroll::calculate_scrollbar(
      sv.viewport_width, sv.content_width, sv.scroll_offset_x,
      style.min_thumb_size);

  float bar_y = viewport_y + viewport_h - style.width;

  // Draw track
  graphics::Rect track_rect{viewport_x, bar_y, viewport_w, style.width};
  graphics::draw_rectangle(track_rect, style.track_color);

  // Draw thumb
  Color thumb_color = style.thumb_color;
  if (style.is_dragging) {
    thumb_color = style.thumb_active_color;
  } else if (style.is_hovering) {
    thumb_color = style.thumb_hover_color;
  }

  graphics::Rect thumb_rect{viewport_x + metrics.thumb_pos, bar_y,
                            metrics.thumb_size, style.width};
  graphics::draw_rectangle(thumb_rect, thumb_color);
}

} // namespace scroll_render

//=============================================================================
// UPDATE FUNCTIONS (call from your render/update loop)
//=============================================================================

namespace scroll {

/// Update scroll view from mouse wheel input
inline void handle_wheel_input(HasScrollView& scroll_view) {
  // Smooth scrolling update
  scroll_view.update_smooth_scroll();

  // Mouse wheel scrolling
  float wheel_y = input_provider::get_mouse_wheel_move();
  if (std::abs(wheel_y) > 0.01f) {
    if (scroll_view.vertical_enabled) {
      scroll_view.scroll_by(0, -wheel_y * scroll_view.scroll_speed);
    } else if (scroll_view.horizontal_enabled) {
      scroll_view.scroll_by(-wheel_y * scroll_view.scroll_speed, 0);
    }
  }

  // 2D trackpad scrolling
  auto wheel_v = input_provider::get_mouse_wheel_move_v();
  if (std::abs(wheel_v.x) > 0.01f || std::abs(wheel_v.y) > 0.01f) {
    float dx = scroll_view.horizontal_enabled
                   ? -wheel_v.x * scroll_view.scroll_speed
                   : 0.f;
    float dy =
        scroll_view.vertical_enabled
            ? -wheel_v.y * scroll_view.scroll_speed
            : 0.f;
    scroll_view.scroll_by(dx, dy);
  }
}

/// Update scrollbar state (hover, drag)
inline void update_scrollbar_state(HasScrollView& scroll_view,
                                   HasScrollbarStyle& style,
                                   float viewport_x, float viewport_y,
                                   float viewport_w, float viewport_h) {
  auto mouse_pos = input_provider::get_mouse_position();
  float mouse_x = mouse_pos.x;
  float mouse_y = mouse_pos.y;

  // Check vertical scrollbar hover
  if (scroll_view.needs_vertical_scrollbar()) {
    float bar_x = viewport_x + viewport_w - style.width;
    bool in_scrollbar = mouse_x >= bar_x && mouse_x < viewport_x + viewport_w &&
                        mouse_y >= viewport_y &&
                        mouse_y < viewport_y + viewport_h;
    style.is_hovering = in_scrollbar;

    // Handle drag
    if (style.is_dragging) {
      if (!input_provider::is_mouse_button_down(0)) {
        style.is_dragging = false;
      } else {
        auto metrics = calculate_scrollbar(
            viewport_h, scroll_view.content_height, scroll_view.scroll_offset_y,
            style.min_thumb_size);
        float drag_pos = mouse_y - viewport_y - metrics.thumb_size / 2.f;
        float new_offset = scrollbar_drag_to_offset(
            drag_pos, viewport_h, metrics.thumb_size,
            scroll_view.content_height, viewport_h);
        scroll_view.scroll_offset_y = std::clamp(new_offset, 0.f,
                                                  scroll_view.max_scroll_y());
      }
    } else if (in_scrollbar && input_provider::is_mouse_button_pressed(0)) {
      style.is_dragging = true;
    }
  }

  // Reset time since scroll for auto-hide
  if (style.is_hovering || style.is_dragging) {
    style.time_since_scroll = 0.f;
  }
}

} // namespace scroll

} // namespace ui
} // namespace afterhours
