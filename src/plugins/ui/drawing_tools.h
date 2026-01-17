// Drawing Tools for Afterhours
// Gap 16: Shape rendering, editing, freeform drawing, z-order management
//
// This is a portable implementation that works in both test and runtime modes.

#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <vector>

#include "../color.h"

namespace afterhours {
namespace ui {

//=============================================================================
// SHAPE RENDERING (ui::draw)
//=============================================================================

namespace draw {

/// Line rendering styles
enum class LineStyle {
  Solid,
  Dashed,
  Dotted,
  DashDot
};

/// Arrow head styles for lines
enum class ArrowStyle {
  None,
  Standard,  // Filled triangle
  Open,      // V-shape
  Diamond,   // Filled diamond
  Circle     // Filled circle
};

} // namespace draw

//=============================================================================
// SHAPE EDITING (ui::edit)
//=============================================================================

namespace edit {

/// Handle positions for shape manipulation
enum class Handle {
  None = 0,
  TopLeft,
  Top,
  TopRight,
  Right,
  BottomRight,
  Bottom,
  BottomLeft,
  Left,
  Rotate  // Handle for rotation
};

/// State of a shape being edited
struct EditState {
  bool selected = false;
  bool dragging = false;
  bool resizing = false;
  bool rotating = false;
  Handle active_handle = Handle::None;
  
  // Drag offset from shape origin
  float drag_offset_x = 0.f;
  float drag_offset_y = 0.f;
  
  // Original bounds before operation
  float original_x = 0.f;
  float original_y = 0.f;
  float original_w = 0.f;
  float original_h = 0.f;
  float original_rotation = 0.f;
  
  void begin_drag(float mouse_x, float mouse_y, float shape_x, float shape_y) {
    dragging = true;
    drag_offset_x = mouse_x - shape_x;
    drag_offset_y = mouse_y - shape_y;
    original_x = shape_x;
    original_y = shape_y;
  }
  
  void begin_resize(float x, float y, float w, float h, Handle handle) {
    resizing = true;
    active_handle = handle;
    original_x = x;
    original_y = y;
    original_w = w;
    original_h = h;
  }
  
  void begin_rotate(float rotation) {
    rotating = true;
    active_handle = Handle::Rotate;
    original_rotation = rotation;
  }
  
  void end_operation() {
    dragging = false;
    resizing = false;
    rotating = false;
    active_handle = Handle::None;
  }
};

/// Hit test a point against a rectangle
inline bool hit_test_rect(float px, float py, float x, float y, float w, float h) {
  return px >= x && px <= x + w && py >= y && py <= y + h;
}

/// Hit test a point against a line segment
inline bool hit_test_line(float px, float py, float x1, float y1, float x2, float y2,
                          float tolerance = 5.f) {
  float dx = x2 - x1;
  float dy = y2 - y1;
  float length_sq = dx * dx + dy * dy;
  
  if (length_sq < 0.001f) {
    float dist = std::sqrt((px - x1) * (px - x1) + (py - y1) * (py - y1));
    return dist <= tolerance;
  }
  
  float t = std::clamp(((px - x1) * dx + (py - y1) * dy) / length_sq, 0.f, 1.f);
  float proj_x = x1 + t * dx;
  float proj_y = y1 + t * dy;
  
  float dist = std::sqrt((px - proj_x) * (px - proj_x) + (py - proj_y) * (py - proj_y));
  return dist <= tolerance;
}

/// Hit test a point against an ellipse
inline bool hit_test_ellipse(float px, float py, float cx, float cy, float rx, float ry) {
  float dx = px - cx;
  float dy = py - cy;
  return (dx * dx) / (rx * rx) + (dy * dy) / (ry * ry) <= 1.f;
}

/// Hit test a point against a triangle
inline bool hit_test_triangle(float px, float py,
                              float x1, float y1, float x2, float y2, float x3, float y3) {
  float d1 = (px - x2) * (y1 - y2) - (x1 - x2) * (py - y2);
  float d2 = (px - x3) * (y2 - y3) - (x2 - x3) * (py - y3);
  float d3 = (px - x1) * (y3 - y1) - (x3 - x1) * (py - y1);

  bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
  bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

  return !(has_neg && has_pos);
}

/// Handle position info
struct HandlePosition {
  Handle handle;
  float x;
  float y;
};

/// Get handle positions for a rectangle
inline std::vector<HandlePosition> get_handle_positions(float x, float y, float w, float h,
                                                        float handle_size = 8.f,
                                                        bool include_rotate = true) {
  std::vector<HandlePosition> handles;
  float hs = handle_size / 2.f;
  
  handles.push_back({Handle::TopLeft, x - hs, y - hs});
  handles.push_back({Handle::Top, x + w / 2.f - hs, y - hs});
  handles.push_back({Handle::TopRight, x + w - hs, y - hs});
  handles.push_back({Handle::Right, x + w - hs, y + h / 2.f - hs});
  handles.push_back({Handle::BottomRight, x + w - hs, y + h - hs});
  handles.push_back({Handle::Bottom, x + w / 2.f - hs, y + h - hs});
  handles.push_back({Handle::BottomLeft, x - hs, y + h - hs});
  handles.push_back({Handle::Left, x - hs, y + h / 2.f - hs});
  
  if (include_rotate) {
    handles.push_back({Handle::Rotate, x + w / 2.f - hs, y - 30.f - hs});
  }
  
  return handles;
}

/// Hit test against selection handles
inline Handle hit_test_handles(float px, float py, float x, float y, float w, float h,
                               float handle_size = 8.f) {
  auto handles = get_handle_positions(x, y, w, h, handle_size, true);
  
  for (const auto& hp : handles) {
    if (hit_test_rect(px, py, hp.x, hp.y, handle_size, handle_size)) {
      return hp.handle;
    }
  }
  
  return Handle::None;
}

/// Update shape position during drag
inline void update_drag(float mouse_x, float mouse_y, EditState& state,
                        float& shape_x, float& shape_y) {
  if (!state.dragging) return;
  
  shape_x = mouse_x - state.drag_offset_x;
  shape_y = mouse_y - state.drag_offset_y;
}

/// Update shape bounds during resize
inline void update_resize(float mouse_x, float mouse_y, EditState& state,
                          float& x, float& y, float& w, float& h,
                          bool maintain_aspect = false) {
  if (!state.resizing) return;
  
  float orig_aspect = (state.original_h > 0) ? state.original_w / state.original_h : 1.f;
  
  switch (state.active_handle) {
    case Handle::TopLeft:
      w = state.original_x + state.original_w - mouse_x;
      h = state.original_y + state.original_h - mouse_y;
      if (maintain_aspect) h = w / orig_aspect;
      x = state.original_x + state.original_w - w;
      y = state.original_y + state.original_h - h;
      break;
      
    case Handle::Top:
      h = state.original_y + state.original_h - mouse_y;
      if (maintain_aspect) w = h * orig_aspect;
      y = state.original_y + state.original_h - h;
      break;
      
    case Handle::TopRight:
      w = mouse_x - state.original_x;
      h = state.original_y + state.original_h - mouse_y;
      if (maintain_aspect) h = w / orig_aspect;
      y = state.original_y + state.original_h - h;
      break;
      
    case Handle::Right:
      w = mouse_x - state.original_x;
      if (maintain_aspect) h = w / orig_aspect;
      break;
      
    case Handle::BottomRight:
      w = mouse_x - state.original_x;
      h = mouse_y - state.original_y;
      if (maintain_aspect) h = w / orig_aspect;
      break;
      
    case Handle::Bottom:
      h = mouse_y - state.original_y;
      if (maintain_aspect) w = h * orig_aspect;
      break;
      
    case Handle::BottomLeft:
      w = state.original_x + state.original_w - mouse_x;
      h = mouse_y - state.original_y;
      if (maintain_aspect) h = w / orig_aspect;
      x = state.original_x + state.original_w - w;
      break;
      
    case Handle::Left:
      w = state.original_x + state.original_w - mouse_x;
      if (maintain_aspect) h = w / orig_aspect;
      x = state.original_x + state.original_w - w;
      break;
      
    default:
      break;
  }
  
  if (w < 10.f) w = 10.f;
  if (h < 10.f) h = 10.f;
}

/// Update shape rotation
inline void update_rotate(float mouse_x, float mouse_y, float center_x, float center_y,
                          EditState& state, float& rotation) {
  if (!state.rotating) return;
  
  float dx = mouse_x - center_x;
  float dy = mouse_y - center_y;
  rotation = std::atan2(dy, dx);
}

/// Snap angle to nearest increment (e.g., 15 degrees)
inline float snap_angle(float angle_rad, float increment_deg = 15.f) {
  float increment_rad = increment_deg * 3.14159265f / 180.f;
  return std::round(angle_rad / increment_rad) * increment_rad;
}

/// Snap position to grid
inline void snap_to_grid(float& x, float& y, float grid_size = 10.f) {
  x = std::round(x / grid_size) * grid_size;
  y = std::round(y / grid_size) * grid_size;
}

} // namespace edit

//=============================================================================
// FREEFORM DRAWING
//=============================================================================

namespace freeform {

/// A single point in a stroke
struct StrokePoint {
  float x;
  float y;
  float pressure = 1.f;
};

/// A complete stroke (path of points)
struct Stroke {
  std::vector<StrokePoint> points;
  Color color{0, 0, 0, 255};
  float thickness = 2.f;
  
  bool empty() const { return points.empty(); }
  
  void add_point(float x, float y, float pressure = 1.f) {
    points.push_back({x, y, pressure});
  }
  
  void clear() { points.clear(); }
};

/// State for ongoing stroke capture
struct StrokeCaptureState {
  bool capturing = false;
  Stroke current_stroke;
  float min_distance = 2.f;
  
  void begin(float x, float y, Color color = {0, 0, 0, 255}, float thickness = 2.f) {
    capturing = true;
    current_stroke.points.clear();
    current_stroke.color = color;
    current_stroke.thickness = thickness;
    current_stroke.add_point(x, y);
  }
  
  void update(float x, float y, float pressure = 1.f) {
    if (!capturing || current_stroke.points.empty()) return;
    
    const auto& last = current_stroke.points.back();
    float dx = x - last.x;
    float dy = y - last.y;
    if (dx * dx + dy * dy >= min_distance * min_distance) {
      current_stroke.add_point(x, y, pressure);
    }
  }
  
  Stroke end() {
    capturing = false;
    Stroke result = std::move(current_stroke);
    current_stroke = Stroke{};
    return result;
  }
  
  void cancel() {
    capturing = false;
    current_stroke.clear();
  }
};

/// Simplify a stroke using Ramer-Douglas-Peucker algorithm
inline void simplify_stroke(Stroke& stroke, float epsilon = 1.f) {
  if (stroke.points.size() < 3) return;
  
  std::vector<bool> keep(stroke.points.size(), false);
  keep.front() = true;
  keep.back() = true;
  
  std::function<void(size_t, size_t)> simplify_range;
  simplify_range = [&](size_t start, size_t end) {
    if (end <= start + 1) return;
    
    const auto& p1 = stroke.points[start];
    const auto& p2 = stroke.points[end];
    
    float max_dist = 0.f;
    size_t max_idx = start;
    
    for (size_t i = start + 1; i < end; ++i) {
      const auto& p = stroke.points[i];
      
      float dx = p2.x - p1.x;
      float dy = p2.y - p1.y;
      float length = std::sqrt(dx * dx + dy * dy);
      
      float dist;
      if (length < 0.001f) {
        dist = std::sqrt((p.x - p1.x) * (p.x - p1.x) + (p.y - p1.y) * (p.y - p1.y));
      } else {
        dist = std::abs((p.x - p1.x) * dy - (p.y - p1.y) * dx) / length;
      }
      
      if (dist > max_dist) {
        max_dist = dist;
        max_idx = i;
      }
    }
    
    if (max_dist > epsilon) {
      keep[max_idx] = true;
      simplify_range(start, max_idx);
      simplify_range(max_idx, end);
    }
  };
  
  simplify_range(0, stroke.points.size() - 1);
  
  std::vector<StrokePoint> simplified;
  for (size_t i = 0; i < stroke.points.size(); ++i) {
    if (keep[i]) {
      simplified.push_back(stroke.points[i]);
    }
  }
  stroke.points = std::move(simplified);
}

} // namespace freeform

//=============================================================================
// Z-ORDER MANAGEMENT (ui::layer)
//=============================================================================

namespace layer {

/// Template wrapper for items with z-order
template <typename T>
struct LayeredItem {
  T item;
  int z_order = 0;
  
  LayeredItem() = default;
  LayeredItem(T i, int z = 0) : item(std::move(i)), z_order(z) {}
};

/// Bring item to front (highest z-order)
template <typename T>
void bring_to_front(std::vector<LayeredItem<T>>& items, size_t index) {
  if (index >= items.size()) return;
  
  int max_z = 0;
  for (const auto& item : items) {
    max_z = std::max(max_z, item.z_order);
  }
  items[index].z_order = max_z + 1;
}

/// Send item to back (lowest z-order)
template <typename T>
void send_to_back(std::vector<LayeredItem<T>>& items, size_t index) {
  if (index >= items.size()) return;
  
  int min_z = 0;
  for (const auto& item : items) {
    min_z = std::min(min_z, item.z_order);
  }
  items[index].z_order = min_z - 1;
}

/// Bring item forward one level
template <typename T>
void bring_forward(std::vector<LayeredItem<T>>& items, size_t index) {
  if (index >= items.size()) return;
  
  int current_z = items[index].z_order;
  int next_z = current_z + 1;
  
  for (auto& item : items) {
    if (item.z_order == next_z) {
      item.z_order = current_z;
      items[index].z_order = next_z;
      return;
    }
  }
  
  items[index].z_order = next_z;
}

/// Send item backward one level
template <typename T>
void send_backward(std::vector<LayeredItem<T>>& items, size_t index) {
  if (index >= items.size()) return;
  
  int current_z = items[index].z_order;
  int prev_z = current_z - 1;
  
  for (auto& item : items) {
    if (item.z_order == prev_z) {
      item.z_order = current_z;
      items[index].z_order = prev_z;
      return;
    }
  }
  
  items[index].z_order = prev_z;
}

/// Sort items by z-order (lowest to highest for rendering)
template <typename T>
void sort_by_z(std::vector<LayeredItem<T>>& items) {
  std::sort(items.begin(), items.end(),
            [](const LayeredItem<T>& a, const LayeredItem<T>& b) {
              return a.z_order < b.z_order;
            });
}

/// Hit test against layered items (returns index of topmost hit, or -1)
template <typename T, typename HitTestFunc>
int hit_test_layered(const std::vector<LayeredItem<T>>& items, float x, float y,
                     HitTestFunc hit_test) {
  int result = -1;
  int max_z = std::numeric_limits<int>::min();
  
  for (size_t i = 0; i < items.size(); ++i) {
    if (hit_test(items[i].item, x, y) && items[i].z_order > max_z) {
      result = static_cast<int>(i);
      max_z = items[i].z_order;
    }
  }
  
  return result;
}

} // namespace layer

//=============================================================================
// TEXT FLOW / SHAPE EXCLUSIONS
//=============================================================================

namespace layout {

/// An exclusion zone that text should flow around
struct Exclusion {
  float x;
  float y;
  float width;
  float height;
  float margin = 4.f;
  
  float left() const { return x - margin; }
  float right() const { return x + width + margin; }
  float top() const { return y - margin; }
  float bottom() const { return y + height + margin; }
  
  bool intersects_line(float line_y, float line_height) const {
    return !(line_y + line_height <= top() || line_y >= bottom());
  }
};

/// A horizontal range of available space
struct AvailableRange {
  float start;
  float end;
  
  float width() const { return end - start; }
};

/// Calculate available horizontal ranges for text on a given line
inline std::vector<AvailableRange> available_ranges_for_line(
    float line_y, float line_height,
    float left_margin, float right_margin,
    const std::vector<Exclusion>& exclusions) {
  
  std::vector<AvailableRange> ranges;
  ranges.push_back({left_margin, right_margin});
  
  for (const auto& excl : exclusions) {
    if (!excl.intersects_line(line_y, line_height)) continue;
    
    std::vector<AvailableRange> new_ranges;
    
    for (const auto& range : ranges) {
      if (excl.right() <= range.start || excl.left() >= range.end) {
        new_ranges.push_back(range);
        continue;
      }
      
      if (excl.left() > range.start) {
        new_ranges.push_back({range.start, excl.left()});
      }
      if (excl.right() < range.end) {
        new_ranges.push_back({excl.right(), range.end});
      }
    }
    
    ranges = std::move(new_ranges);
  }
  
  return ranges;
}

} // namespace layout

} // namespace ui
} // namespace afterhours
