#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "../../drawing_helpers.h"
#include "../../ecs.h"
#include "../../font_helper.h"
#include "../../logging.h"
#include "../animation.h"
#ifdef AFTER_HOURS_ENABLE_E2E_TESTING
#include "../e2e_testing/test_input.h"
#include "../e2e_testing/visible_text.h"
#endif
#include "../../memory/arena.h"
#include "../input_system.h"
#include "../texture_manager.h"
#include "animation_keys.h"
#include "components.h"
#include "context.h"
#include "fmt/format.h"
#include "render_primitives.h"
#include "systems.h"
#include "theme.h"

namespace afterhours {

namespace ui {

// Left-side bearing is now calculated per-string using
// get_first_glyph_bearing() in font_helper.h. No more hardcoded offset.

namespace detail {

static inline float compute_effective_opacity(const Entity &entity) {
  float result = 1.0f;
  EntityID current_id = entity.id;
  int guard = 0;
  while (current_id >= 0 && guard < 64) {
    OptEntity opt_cur = UICollectionHolder::getEntityForID(current_id);
    if (!opt_cur.valid())
      break;
    const Entity &cur = opt_cur.asE();
    if (cur.has<HasOpacity>()) {
      result *= std::clamp(cur.get<HasOpacity>().value, 0.0f, 1.0f);
    }
    if (!cur.has<UIComponent>())
      break;
    EntityID pid = cur.get<UIComponent>().parent;
    if (pid < 0 || pid == current_id)
      break;
    current_id = pid;
    ++guard;
  }
  return std::clamp(result, 0.0f, 1.0f);
}

// Find the nearest ancestor with HasScrollView or HasClipChildren
// Returns invalid OptEntity if no clipping ancestor exists
static inline OptEntity find_clip_ancestor(const Entity &entity) {
  if (!entity.has<UIComponent>())
    return {};

  const UIComponent &cmp = entity.get<UIComponent>();
  EntityID pid = cmp.parent;

  int guard = 0;
  while (pid >= 0 && guard < 64) {
    OptEntity opt_parent = UICollectionHolder::getEntityForID(pid);
    if (!opt_parent.valid()) {
      break;
    }
    Entity &parent = opt_parent.asE();

    if (parent.has<HasScrollView>() || parent.has<HasClipChildren>()) {
      return opt_parent;
    }
    if (!parent.has<UIComponent>())
      break;
    pid = parent.get<UIComponent>().parent;
    ++guard;
  }
  return {};
}

// Legacy alias for backwards compatibility
static inline OptEntity find_scroll_view_ancestor(const Entity &entity) {
  return find_clip_ancestor(entity);
}

// Get scroll offset from ancestor scroll view, returns {0,0} if none
static inline Vector2Type get_scroll_offset(const Entity &entity) {
  OptEntity scroll_ancestor = find_scroll_view_ancestor(entity);
  if (scroll_ancestor.valid() && scroll_ancestor->has<HasScrollView>()) {
    return scroll_ancestor->get<HasScrollView>().scroll_offset;
  }
  return {0.0f, 0.0f};
}

// Get the scissor rect from a scroll view ancestor (viewport bounds)
static inline RectangleType get_scroll_scissor_rect(const Entity &entity) {
  OptEntity scroll_ancestor = find_scroll_view_ancestor(entity);
  if (scroll_ancestor.valid() && scroll_ancestor->has<UIComponent>()) {
    return scroll_ancestor->get<UIComponent>().rect();
  }
  return {0, 0, 0, 0};
}

// Recompute children's positions for scroll view containers
// The layout system constrains children to parent bounds, which stacks overflow
// items at the same position. This function fixes that by sequentially
// positioning children based on their sizes.
static inline void fix_scroll_view_child_positions(Entity &entity) {
  if (!entity.has<HasScrollView>() || !entity.has<UIComponent>())
    return;

  UIComponent &cmp = entity.get<UIComponent>();
  HasScrollView &scroll = entity.get<HasScrollView>();
  RectangleType parent_rect = cmp.rect();

  // Starting position for children (inside parent's content area)
  float content_x = parent_rect.x + cmp.computed_padd[Axis::left];
  float content_y = parent_rect.y + cmp.computed_padd[Axis::top];

  // Determine layout direction based on which scrolling is enabled
  // If horizontal scrolling enabled, assume row layout
  bool is_row_layout = scroll.horizontal_enabled && !scroll.vertical_enabled;

  float current_x = content_x;
  float current_y = content_y;

  for (EntityID child_id : cmp.children) {
    OptEntity child_opt = UICollectionHolder::getEntityForID(child_id);
    if (!child_opt.valid())
      continue;

    Entity &child = child_opt.asE();
    if (!child.has<UIComponent>())
      continue;

    UIComponent &child_cmp = child.get<UIComponent>();

    float child_margin_top = child_cmp.computed_margin[Axis::top];
    float child_margin_left = child_cmp.computed_margin[Axis::left];
    float child_margin_bottom = child_cmp.computed_margin[Axis::bottom];
    float child_margin_right = child_cmp.computed_margin[Axis::right];

    if (is_row_layout) {
      // Row layout: position horizontally
      child_cmp.computed_rel[Axis::X] = current_x + child_margin_left;
      child_cmp.computed_rel[Axis::Y] = content_y + child_margin_top;

      // Move right for next child
      float child_width = child_cmp.computed[Axis::X];
      current_x += child_margin_left + child_width + child_margin_right;
    } else {
      // Column layout: position vertically
      child_cmp.computed_rel[Axis::X] = content_x + child_margin_left;
      child_cmp.computed_rel[Axis::Y] = current_y + child_margin_top;

      // Move down for next child
      float child_height = child_cmp.computed[Axis::Y];
      current_y += child_margin_top + child_height + child_margin_bottom;
    }
  }
}

// Compute content size for a scroll view from its children's sizes
// For scroll views, we sum children's sizes instead of using screen positions
// because the layout system constrains children to the viewport
static inline void update_scroll_view_content_size(Entity &entity) {
  if (!entity.has<HasScrollView>() || !entity.has<UIComponent>())
    return;

  HasScrollView &scroll = entity.get<HasScrollView>();
  const UIComponent &cmp = entity.get<UIComponent>();

  // Start with viewport size from the rect (accounts for margins/padding)
  RectangleType parent_rect = cmp.rect();
  scroll.viewport_size = {parent_rect.width, parent_rect.height};

  // Determine layout direction based on which scrolling is enabled
  bool is_row_layout = scroll.horizontal_enabled && !scroll.vertical_enabled;

  float total_width = 0.0f;
  float total_height = 0.0f;
  float max_width = 0.0f;
  float max_height = 0.0f;

  for (EntityID child_id : cmp.children) {
    OptEntity child_opt = UICollectionHolder::getEntityForID(child_id);
    if (!child_opt.valid())
      continue;

    Entity &child = child_opt.asE();
    if (!child.has<UIComponent>())
      continue;

    const UIComponent &child_cmp = child.get<UIComponent>();

    float child_width = child_cmp.computed[Axis::X];
    float child_height = child_cmp.computed[Axis::Y];
    float child_margin_left = child_cmp.computed_margin[Axis::left];
    float child_margin_right = child_cmp.computed_margin[Axis::right];
    float child_margin_top = child_cmp.computed_margin[Axis::top];
    float child_margin_bottom = child_cmp.computed_margin[Axis::bottom];

    float child_total_width =
        child_width + child_margin_left + child_margin_right;
    float child_total_height =
        child_height + child_margin_top + child_margin_bottom;

    if (is_row_layout) {
      // Row layout: sum widths, take max height
      total_width += child_total_width;
      max_height = std::max(max_height, child_total_height);
    } else {
      // Column layout: sum heights, take max width
      total_height += child_total_height;
      max_width = std::max(max_width, child_total_width);
    }
  }

  if (is_row_layout) {
    scroll.content_size = {total_width, max_height};
  } else {
    scroll.content_size = {max_width, total_height};
  }

  // In auto mode, only fix child positions when content actually overflows.
  // When content fits, the layout engine's positions are correct.
  bool content_overflows = scroll.needs_scroll_y() || scroll.needs_scroll_x();
  if (!scroll.auto_overflow || content_overflows) {
    fix_scroll_view_child_positions(entity);
  }

  scroll.clamp_scroll();
}

} // namespace detail

// Minimum font size to prevent invalid rendering (font size 0)
// This ensures text is always readable - 10px is the practical minimum
constexpr float MIN_FONT_SIZE = 10.0f;
// Font size threshold for debug visualization - text is likely unreadable
constexpr float DEBUG_FONT_SIZE_THRESHOLD = 8.0f;

// Convert CursorType to backend mouse cursor ID
// Values match across raylib and sokol backends
inline int to_cursor_id(CursorType cursor) {
  switch (cursor) {
  case CursorType::Default:
    return 0; // MOUSE_CURSOR_DEFAULT
  case CursorType::Pointer:
    return 4; // MOUSE_CURSOR_POINTING_HAND
  case CursorType::Text:
    return 2; // MOUSE_CURSOR_IBEAM
  case CursorType::ResizeH:
    return 5; // MOUSE_CURSOR_RESIZE_EW
  case CursorType::ResizeV:
    return 6; // MOUSE_CURSOR_RESIZE_NS
  }
  return 0;
}

// Enable visual debug indicators for text that can't fit in containers
// Define AFTERHOURS_DEBUG_TEXT_OVERFLOW to show red corner indicators
#ifdef AFTERHOURS_DEBUG_TEXT_OVERFLOW
constexpr bool SHOW_TEXT_OVERFLOW_DEBUG = true;
#else
constexpr bool SHOW_TEXT_OVERFLOW_DEBUG = false;
#endif

// Result struct for position_text that includes whether text fits properly
struct TextPositionResult {
  RectangleType rect;
  bool text_fits; // false if font was clamped to minimum (text won't fit)
};

static inline TextPositionResult
position_text_ex(const ui::FontManager &fm, const std::string &text,
                 RectangleType container, TextAlignment alignment,
                 Vector2Type margin_px, float explicit_font_size = 0.f,
                 float extra_spacing = 0.f,
                 TextOverflow text_overflow = TextOverflow::Clip) {
  // Early return for empty text - prevents infinite loop in font size
  // calculation
  if (text.empty()) {
    return TextPositionResult{
        .rect =
            RectangleType{
                .x = container.x + margin_px.x,
                .y = container.y + margin_px.y,
                .width = MIN_FONT_SIZE,
                .height = MIN_FONT_SIZE,
            },
        .text_fits = true,
    };
  }

  Font font = fm.get_active_font();
  float bearing = get_first_glyph_bearing(font, text.c_str());

  // Calculate the maximum text size based on the container size and margins
  Vector2Type max_text_size = Vector2Type{
      .x = container.width - 2 * margin_px.x,
      .y = container.height - 2 * margin_px.y,
  };

  // Check for invalid container (negative or zero usable space)
  if (max_text_size.x <= 0 || max_text_size.y <= 0) {
#ifdef AFTERHOURS_DEBUG_TEXT_OVERFLOW
    // Only warn when overflow persists across multiple frames. Elements with
    // percent/absolute sizing may have zero dimensions on their first layout
    // frame before the autolayout pass resolves them.
    static std::unordered_map<std::string, int> overflow_frame_count;
    overflow_frame_count[text]++;
    if (overflow_frame_count[text] == 3) {
      log_warn("Container too small for text: container={}x{}, margins={}x{}, "
               "text='{}'",
               container.width, container.height, margin_px.x, margin_px.y,
               text.length() > 20 ? text.substr(0, 20) + "..." : text);
    }
#endif
    return TextPositionResult{
        .rect =
            RectangleType{
                .x = container.x + margin_px.x,
                .y = container.y + margin_px.y,
                .width = MIN_FONT_SIZE,
                .height = MIN_FONT_SIZE,
            },
        .text_fits = false,
    };
  }
  // TODO add some caching here?

  float font_size;
  bool text_fits;

  if (explicit_font_size > 0.f) {
    // When an explicit font size is provided, use it directly instead of
    // auto-sizing. Text may overflow the container horizontally; the caller
    // is responsible for ensuring the size is appropriate.
    font_size = std::max(explicit_font_size, MIN_FONT_SIZE);
    text_fits = true;
  } else {
    // Use binary search to find largest font size that fits
    float low = MIN_FONT_SIZE;
    float high = std::min(max_text_size.y, 200.f); // Cap at reasonable max
    font_size = low;

    // When text_overflow is Ellipsis, text will be truncated to fit width,
    // so only constrain font size by height.
    const bool width_constrained = (text_overflow != TextOverflow::Ellipsis);

    while (high - low > 0.5f) {
      float mid = (low + high) / 2.f;
      Vector2Type ts =
          measure_text(font, text.c_str(), mid, 1.f + extra_spacing);
      bool fits = ts.y <= max_text_size.y &&
                  (!width_constrained || ts.x <= max_text_size.x);
      if (fits) {
        font_size = mid;
        low = mid;
      } else {
        high = mid;
      }
    }

    // Clamp to minimum font size to prevent invalid rendering
    text_fits = font_size >= MIN_FONT_SIZE;
    if (font_size < MIN_FONT_SIZE) {
#ifdef AFTERHOURS_DEBUG_TEXT_OVERFLOW
      // Only log once per unique text to avoid spamming
      static std::unordered_set<std::string> logged_texts;
      if (logged_texts.find(text) == logged_texts.end()) {
        logged_texts.insert(text);
        log_warn("Text '{}' cannot fit in container {}x{} with margins {}x{} - "
                 "clamping font size from {} to {}",
                 text.length() > 20 ? text.substr(0, 20) + "..." : text,
                 container.width, container.height, margin_px.x, margin_px.y,
                 font_size, MIN_FONT_SIZE);
      }
#endif
      font_size = MIN_FONT_SIZE;
    }
  }

  // Measure with final font size for accurate positioning
  Vector2Type text_size =
      measure_text(font, text.c_str(), font_size, 1.f + extra_spacing);

  // Calculate the text position based on the alignment and margins
  Vector2Type position;
  switch (alignment) {
  default:
    log_warn("Unknown alignment: {}", static_cast<int>(alignment));
    [[fallthrough]];
  case TextAlignment::None: // None defaults to Left alignment
  case TextAlignment::Left:
    position = Vector2Type{
        .x = container.x + margin_px.x + bearing,
        .y = container.y + margin_px.y +
             (container.height - 2 * margin_px.y - text_size.y) / 2,
    };
    break;
  case TextAlignment::Center: {
    // Calculate centered position, but clamp to prevent starting before
    // container left edge
    float centered_offset =
        (container.width - 2 * margin_px.x - text_size.x) / 2;
    float text_x = container.x + margin_px.x + centered_offset;
    // Clamp so text never starts before container left edge
    text_x = std::max(container.x + margin_px.x, text_x);
    position = Vector2Type{
        .x = text_x,
        .y = container.y + margin_px.y +
             (container.height - 2 * margin_px.y - text_size.y) / 2,
    };
    break;
  }
  case TextAlignment::Right:
    position = Vector2Type{
        .x = container.x + container.width - margin_px.x - text_size.x,
        .y = container.y + margin_px.y +
             (container.height - 2 * margin_px.y - text_size.y) / 2,
    };
    break;
  }

  return TextPositionResult{
      .rect =
          RectangleType{
              .x = position.x,
              .y = position.y,
              .width = font_size,
              .height = font_size,
          },
      .text_fits = text_fits,
  };
}

// Backwards-compatible wrapper that returns just the rectangle
static inline RectangleType position_text(const ui::FontManager &fm,
                                          const std::string &text,
                                          RectangleType container,
                                          TextAlignment alignment,
                                          Vector2Type margin_px) {
  return position_text_ex(fm, text, container, alignment, margin_px).rect;
}

// Internal helper to draw text at a specific position (used by stroke, shadow,
// and main text) The 'sizing' rect contains any offset (shadow/stroke) that
// should be applied
namespace detail {
static inline void
draw_text_at_position(const ui::FontManager &fm, const std::string &text,
                      RectangleType rect, TextAlignment alignment,
                      RectangleType sizing, Color color, float rotation = 0.0f,
                      float rot_center_x = 0.0f, float rot_center_y = 0.0f,
                      float extra_spacing = 0.0f) {
  // Always use UTF-8 aware rendering (works for all text including CJK)
  Font font = fm.get_active_font();
  float fontSize = sizing.height;
  float spacing = 1.0f + extra_spacing;

  // position_text_ex already computed alignment-aware position in sizing.x/y.
  // Shadow/stroke offsets are also pre-applied to sizing before this call.
  // Just use the pre-calculated position directly.
  Vector2Type startPos = {sizing.x, sizing.y};

  // Use provided rotation center (component center), or default to text rect
  // center
  float centerX = (rot_center_x != 0.0f || rot_center_y != 0.0f)
                      ? rot_center_x
                      : rect.x + rect.width / 2.0f;
  float centerY = (rot_center_x != 0.0f || rot_center_y != 0.0f)
                      ? rot_center_y
                      : rect.y + rect.height / 2.0f;
  draw_text_ex(font, text.c_str(), startPos, fontSize, spacing, color, rotation,
               centerX, centerY);
}
} // namespace detail

static inline void draw_text_in_rect(
    const ui::FontManager &fm, const std::string &text, RectangleType rect,
    TextAlignment alignment, Color color, bool show_debug_indicator = false,
    const std::optional<TextStroke> &stroke = std::nullopt,
    const std::optional<TextShadow> &shadow = std::nullopt,
    float rotation = 0.0f, float rot_center_x = 0.0f, float rot_center_y = 0.0f,
    TextOverflow text_overflow = TextOverflow::Clip,
    float letter_spacing = 0.0f,
    float explicit_font_size = 0.0f) {
#ifdef AFTER_HOURS_ENABLE_E2E_TESTING
  // Register text for E2E testing assertions (only visible-in-viewport text)
  if (testing::test_input::detail::test_mode) {
    auto *pcr = EntityHelper::get_singleton_cmp<
        window_manager::ProvidesCurrentResolution>();
    float vw = pcr ? static_cast<float>(pcr->width()) : 1280.f;
    float vh = pcr ? static_cast<float>(pcr->height()) : 720.f;
    testing::VisibleTextRegistry::instance().register_text_if_visible(
        text, rect.x, rect.y, rect.width, rect.height, vw, vh);
  }
#endif

  TextPositionResult result = [&]() {
    Vector2Type margin_px{5.f, 5.f};
    if (rect.width <= 0.0f || rect.height <= 0.0f) {
      margin_px = Vector2Type{0.0f, 0.0f};
    } else {
      margin_px.x = std::min(margin_px.x, rect.width * 0.4f);
      margin_px.y = std::min(margin_px.y, rect.height * 0.4f);
    }
    return position_text_ex(fm, text, rect, alignment, margin_px,
                            explicit_font_size, letter_spacing, text_overflow);
  }();

  // Draw visual debug indicator if text doesn't fit and debug is enabled
  // Shows a semi-transparent red overlay and border around the container
  if (show_debug_indicator && !result.text_fits) {
    Color overlay_color = Color{255, 50, 50, 60}; // Semi-transparent red fill
    Color border_color = Color{255, 50, 50, 200}; // Solid red border
    float border_thickness = 2.0f;

    // Draw semi-transparent red overlay on the entire container
    draw_rectangle(rect, overlay_color);

    // Draw red border lines around the container
    // Top
    draw_rectangle(RectangleType{.x = rect.x,
                                 .y = rect.y,
                                 .width = rect.width,
                                 .height = border_thickness},
                   border_color);
    // Bottom
    draw_rectangle(RectangleType{.x = rect.x,
                                 .y = rect.y + rect.height - border_thickness,
                                 .width = rect.width,
                                 .height = border_thickness},
                   border_color);
    // Left
    draw_rectangle(RectangleType{.x = rect.x,
                                 .y = rect.y,
                                 .width = border_thickness,
                                 .height = rect.height},
                   border_color);
    // Right
    draw_rectangle(RectangleType{.x = rect.x + rect.width - border_thickness,
                                 .y = rect.y,
                                 .width = border_thickness,
                                 .height = rect.height},
                   border_color);
  }

  // Don't attempt to render if font size is effectively zero
  if (result.rect.height < MIN_FONT_SIZE) {
    return;
  }

  // Handle text overflow ellipsis truncation
  std::string truncated_text;
  const std::string &render_text = [&]() -> const std::string & {
    if (text_overflow != TextOverflow::Ellipsis || text.empty()) {
      return text;
    }
    Font font = fm.get_active_font();
    float font_size = result.rect.height;
    float spacing = 1.f + letter_spacing;
    float max_width = rect.width - 10.f; // Account for margins (5px each side)
    if (max_width <= 0.f)
      return text;

    Vector2Type text_size =
        measure_text(font, text.c_str(), font_size, spacing);
    if (text_size.x <= max_width) {
      return text;
    }

    // Text overflows — find longest prefix that fits with "..."
    const std::string ellipsis = "...";
    Vector2Type ellipsis_size =
        measure_text(font, ellipsis.c_str(), font_size, spacing);
    float available = max_width - ellipsis_size.x;
    if (available <= 0.f) {
      truncated_text = ellipsis;
      return truncated_text;
    }

    // Binary search for the longest prefix that fits
    size_t low = 0;
    size_t high = text.size();
    size_t best = 0;
    while (low <= high && high <= text.size()) {
      size_t mid = (low + high) / 2;
      std::string prefix = text.substr(0, mid);
      Vector2Type ps = measure_text(font, prefix.c_str(), font_size, spacing);
      if (ps.x <= available) {
        best = mid;
        low = mid + 1;
      } else {
        if (mid == 0)
          break;
        high = mid - 1;
      }
    }

    truncated_text = text.substr(0, best) + ellipsis;
    return truncated_text;
  }();

  RectangleType sizing = result.rect;

  // Draw text shadow first (behind everything)
  // Renders text at a single offset position to create a drop shadow effect
  if (shadow.has_value() && shadow->has_shadow()) {
    RectangleType shadow_sizing = sizing;
    shadow_sizing.x += shadow->offset_x;
    shadow_sizing.y += shadow->offset_y;
    detail::draw_text_at_position(fm, render_text, rect, alignment,
                                  shadow_sizing, shadow->color, rotation,
                                  rot_center_x, rot_center_y, letter_spacing);
  }

  // Draw text stroke/outline if configured
  // Renders text at 8 offset positions to create an outline effect
  if (stroke.has_value() && stroke->has_stroke()) {
    float t = stroke->thickness;
    Color stroke_color = stroke->color;

    // 8-direction offsets for stroke rendering
    const std::array<std::pair<float, float>, 8> offsets = {
        {{-t, -t}, {0, -t}, {t, -t}, {-t, 0}, {t, 0}, {-t, t}, {0, t}, {t, t}}};

    for (const auto &[ox, oy] : offsets) {
      RectangleType offset_sizing = sizing;
      offset_sizing.x += ox;
      offset_sizing.y += oy;
      detail::draw_text_at_position(fm, render_text, rect, alignment,
                                    offset_sizing, stroke_color, rotation,
                                    rot_center_x, rot_center_y, letter_spacing);
    }
  }

  // Draw main text on top
  detail::draw_text_at_position(fm, render_text, rect, alignment, sizing, color,
                                rotation, rot_center_x, rot_center_y,
                                letter_spacing);
}

static inline Vector2Type
position_texture(texture_manager::Texture, Vector2Type size,
                 RectangleType container,
                 texture_manager::HasTexture::Alignment alignment,
                 Vector2Type margin_px = {0.f, 0.f}) {
  // Calculate the text position based on the alignment and margins
  Vector2Type position;

  switch (alignment) {
  case texture_manager::HasTexture::Alignment::Left:
    position = Vector2Type{
        .x = container.x + margin_px.x,
        .y = container.y + margin_px.y + size.x,
    };
    break;
  case texture_manager::HasTexture::Alignment::Center:
    position = Vector2Type{
        .x = container.x + margin_px.x + (container.width / 2) + (size.x / 2),
        .y = container.y + margin_px.y + (container.height / 2) + (size.y / 2),
    };
    break;
  case texture_manager::HasTexture::Alignment::Right:
    position = Vector2Type{
        .x = container.x + container.width - margin_px.x + size.x,
        .y = container.y + margin_px.y + size.y,
    };
    break;
  default:
    // Handle unknown alignment (shouldn't happen)
    break;
  }

  return Vector2Type{
      .x = position.x,
      .y = position.y,
  };
}

static inline void
draw_texture_in_rect(texture_manager::Texture texture, RectangleType rect,
                     texture_manager::HasTexture::Alignment alignment) {
  float scale = (float)texture.height / rect.height;
  Vector2Type size = {
      (float)texture.width / scale,
      (float)texture.height / scale,
  };

  Vector2Type location = position_texture(texture, size, rect, alignment);

  texture_manager::draw_texture_pro(texture,
                                    RectangleType{
                                        0.0f,
                                        0.0f,
                                        (float)texture.width,
                                        (float)texture.height,
                                    },
                                    RectangleType{
                                        .x = location.x,
                                        .y = location.y,
                                        .width = size.x,
                                        .height = size.y,
                                    },
                                    size, 0.f, colors::UI_WHITE);
}

template <typename InputAction>
struct RenderDebugAutoLayoutRoots : SystemWithUIContext<AutoLayoutRoot> {
  InputAction toggle_action;
  bool enabled = false;
  float enableCooldown = 0.f;
  float enableCooldownReset = 0.2f;

  UIContext<InputAction> *context;

  int level = 0;
  int indent = 0;
  EntityID isolated_id = -1;
  bool isolate_enabled = false;
  enum struct IsolationMode { NodeOnly, NodeAndDescendants };
  IsolationMode isolation_mode = IsolationMode::NodeOnly;
  UIEntityMappingCache *cache = nullptr;

  float fontSize = 20.0f;

  RenderDebugAutoLayoutRoots(InputAction toggle_kp) : toggle_action(toggle_kp) {
    this->include_derived_children = true;
  }

  virtual ~RenderDebugAutoLayoutRoots() {}

  virtual bool should_run(float dt) override {
    // Don't run if cache singleton doesn't exist yet
    if (!EntityHelper::get_singleton_cmp<UIEntityMappingCache>()) {
      return false;
    }

    enableCooldown -= dt;

    if (enableCooldown < 0) {
      enableCooldown = enableCooldownReset;
      input::PossibleInputCollector inpc = input::get_input_collector();
      for (auto &actions_done : inpc.inputs()) {
        if (static_cast<InputAction>(actions_done.action) == toggle_action) {
          enabled = !enabled;
          break;
        }
      }
    }
    return enabled;
  }

  virtual void once(float) override {
    this->context =
        EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();

    // Get cached entity mapping
    this->cache = EntityHelper::get_singleton_cmp<UIEntityMappingCache>();

    draw_text(fmt::format("mouse({}, {})", this->context->mouse.pos.x,
                          this->context->mouse.pos.y)
                  .c_str(),
              0.0f, 0.0f, fontSize,
              this->context->theme.from_usage(Theme::Usage::Font));

    // starting at 1 to avoid the mouse text
    this->level = 1;
    this->indent = 0;
  }

  bool is_descendant_of_isolated(const Entity &entity) const {
    if (!isolate_enabled)
      return false;
    if (entity.id == isolated_id)
      return false;
    EntityID current_id = entity.id;
    int guard = 0;
    while (guard < 64) {
      OptEntity opt_cur = UICollectionHolder::getEntityForID(current_id);
      if (!opt_cur.valid() || !opt_cur.asE().has<UIComponent>())
        break;
      const UIComponent &cur_cmp = opt_cur.asE().get<UIComponent>();
      if (cur_cmp.parent < 0)
        break;
      if (cur_cmp.parent == isolated_id)
        return true;
      current_id = cur_cmp.parent;
      ++guard;
    }
    return false;
  }

  void render_me(const Entity &entity) {
    const UIComponent &cmp = entity.get<UIComponent>();

    const float x = 10 * indent;
    const float y = (fontSize * level) + fontSize / 2.f;

    std::string component_name = "Unknown";
    if (entity.has<UIComponentDebug>()) {
      const auto &cmpdebug = entity.get<UIComponentDebug>();
      component_name = cmpdebug.name();
    }

    const std::string widget_str = fmt::format(
        "{:03} (x{:05.2f} y{:05.2f}) w{:05.2f}xh{:05.2f} {}", (int)entity.id,
        cmp.x(), cmp.y(), cmp.rect().width, cmp.rect().height, component_name);

    const float text_width =
        measure_text_internal(widget_str.c_str(), fontSize);
    const Rectangle debug_label_location =
        Rectangle{x, y, text_width, fontSize};

    const bool is_hovered =
        is_mouse_inside(this->context->mouse.pos, debug_label_location);
    bool show = true;
    if (isolate_enabled) {
      if (entity.id == isolated_id) {
        show = true;
      } else if (isolation_mode == IsolationMode::NodeAndDescendants) {
        show = is_descendant_of_isolated(entity);
      } else {
        show = false;
      }
    }
    const bool hidden = !show;

    const auto color_or_hidden = [hidden](Color c) {
      return hidden ? colors::opacity_pct(c, 0.f) : c;
    };

    if (is_hovered) {
      draw_rectangle_outline(cmp.rect(),
                             color_or_hidden(this->context->theme.from_usage(
                                 Theme::Usage::Error)));
      draw_rectangle_outline(cmp.bounds(), color_or_hidden(colors::UI_BLACK));
      draw_rectangle(debug_label_location, color_or_hidden(colors::UI_BLUE));
    } else {
      draw_rectangle(debug_label_location, color_or_hidden(colors::UI_BLACK));
    }

    Color baseText = this->context->is_hot(entity.id)
                         ? this->context->theme.from_usage(Theme::Usage::Error)
                         : this->context->theme.from_usage(Theme::Usage::Font);
    draw_text(widget_str.c_str(), x, y, fontSize, color_or_hidden(baseText));

    const bool left_released = input::is_mouse_button_released(0);
    const bool right_released = input::is_mouse_button_released(1);
    if (is_hovered && (left_released || right_released)) {
      IsolationMode new_mode = left_released ? IsolationMode::NodeAndDescendants
                                             : IsolationMode::NodeOnly;
      if (isolate_enabled && isolated_id == entity.id &&
          isolation_mode == new_mode) {
        isolate_enabled = false;
        isolated_id = -1;
      } else {
        isolate_enabled = true;
        isolated_id = entity.id;
        isolation_mode = new_mode;
      }
    }
  }

  void render(const Entity &entity) {
    const UIComponent &cmp = entity.get<UIComponent>();
    if (cmp.should_hide)
      return;

    if (cmp.was_rendered_to_screen) {
      render_me(entity);
      level++;
    }

    indent++;
    for (EntityID child : cmp.children) {
      render(cache->to_ent(child));
    }
    indent--;
  }

  virtual void for_each_with_derived(Entity &entity, UIComponent &,
                                     AutoLayoutRoot &, float) {
    render(entity);
    level += 2;
    indent = 0;
  }
};

template <typename InputAction>
struct RenderImm : System<UIContext<InputAction>, FontManager> {
  RenderImm() : System<UIContext<InputAction>, FontManager>() {
    this->include_derived_children = true;
  }

  void render_shadow(const Entity &entity, RectangleType draw_rect,
                     const std::bitset<4> &corner_settings,
                     float effective_opacity, float roundness = 0.5f,
                     int segments = 8) {
    if (!entity.has<HasShadow>())
      return;

    const Shadow &shadow = entity.get<HasShadow>().shadow;
    Color shadow_color = shadow.color;
    if (effective_opacity < 1.0f) {
      shadow_color.a =
          static_cast<unsigned char>(shadow_color.a * effective_opacity);
    }

    if (shadow.style == ShadowStyle::Hard) {
      // Hard shadow: single offset rectangle
      RectangleType shadow_rect = {draw_rect.x + shadow.offset_x,
                                   draw_rect.y + shadow.offset_y,
                                   draw_rect.width, draw_rect.height};
      if (corner_settings.any()) {
        draw_rectangle_rounded(shadow_rect, roundness, segments, shadow_color,
                               corner_settings);
      } else {
        draw_rectangle(shadow_rect, shadow_color);
      }
    } else {
      // Soft shadow: layered rectangles for blur effect
      int layers = static_cast<int>(shadow.blur_radius / 2.0f);
      layers = std::max(3, std::min(layers, 8)); // Clamp between 3-8 layers

      for (int i = layers; i >= 0; --i) {
        float spread = shadow.blur_radius *
                       (static_cast<float>(i) / static_cast<float>(layers));
        float alpha_factor =
            1.0f - (static_cast<float>(i) / static_cast<float>(layers + 1));

        RectangleType shadow_rect = {
            draw_rect.x + shadow.offset_x - spread * 0.5f,
            draw_rect.y + shadow.offset_y - spread * 0.5f,
            draw_rect.width + spread, draw_rect.height + spread};

        Color layer_color = shadow_color;
        layer_color.a = static_cast<unsigned char>(
            static_cast<float>(shadow_color.a) * alpha_factor *
            (1.0f / static_cast<float>(layers)));

        if (corner_settings.any()) {
          draw_rectangle_rounded(shadow_rect, roundness, segments, layer_color,
                                 corner_settings);
        } else {
          draw_rectangle(shadow_rect, layer_color);
        }
      }
    }
  }

  void render_nine_slice(const Entity &entity, RectangleType draw_rect,
                         float effective_opacity) {
    if (!entity.has<HasNineSliceBorder>())
      return;

    const NineSliceBorder &nine_slice =
        entity.get<HasNineSliceBorder>().nine_slice;
    Color tint = nine_slice.tint;
    if (effective_opacity < 1.0f) {
      tint.a = static_cast<unsigned char>(tint.a * effective_opacity);
    }

    draw_texture_npatch(nine_slice.texture, draw_rect, nine_slice.left,
                        nine_slice.top, nine_slice.right, nine_slice.bottom,
                        tint);
  }

  void render_circular_progress(const Entity &entity, RectangleType draw_rect,
                                float effective_opacity) {
    if (!entity.has<HasCircularProgressState>()) {
      return;
    }

    const HasCircularProgressState &state =
        entity.get<HasCircularProgressState>();

    // Calculate center and radius from the draw_rect
    float centerX = draw_rect.x + draw_rect.width / 2.0f;
    float centerY = draw_rect.y + draw_rect.height / 2.0f;
    float outerRadius = std::min(draw_rect.width, draw_rect.height) / 2.0f;
    float innerRadius = outerRadius - state.thickness;
    if (innerRadius < 0.0f)
      innerRadius = 0.0f;

    // Apply opacity to colors
    Color track_color = state.track_color;
    Color fill_color = state.fill_color;
    if (effective_opacity < 1.0f) {
      track_color = colors::opacity_pct(track_color, effective_opacity);
      fill_color = colors::opacity_pct(fill_color, effective_opacity);
    }

    // Calculate segments based on radius for smoothness
    int segments = std::max(32, static_cast<int>(outerRadius * 0.5f));

    // Draw background track (full circle)
    draw_ring(centerX, centerY, innerRadius, outerRadius, segments,
              track_color);

    // Draw progress fill (arc from start_angle)
    if (state.value > 0.001f) {
      float end_angle = state.start_angle + (state.value * 360.0f);
      draw_ring_segment(centerX, centerY, innerRadius, outerRadius,
                        state.start_angle, end_angle, segments, fill_color);
    }
  }

  void render_bevel(const Entity &entity, RectangleType draw_rect,
                    float effective_opacity) {
    if (!entity.has<HasBevelBorder>())
      return;

    const BevelBorder &bevel = entity.get<HasBevelBorder>().bevel;
    if (!bevel.has_bevel())
      return;

    Color light = bevel.light_color;
    Color dark = bevel.dark_color;
    if (effective_opacity < 1.0f) {
      light = colors::opacity_pct(light, effective_opacity);
      dark = colors::opacity_pct(dark, effective_opacity);
    }

    Color base_fill = colors::UI_WHITE;
    if (entity.has<HasColor>()) {
      base_fill = entity.get<HasColor>().color();
    }
    if (effective_opacity < 1.0f) {
      base_fill = colors::opacity_pct(base_fill, effective_opacity);
    }

    const Color strong_light = light;
    const Color strong_dark = dark;
    const Color mid_light = colors::lighten(base_fill, 0.35f);
    const Color mid_dark = colors::darken(base_fill, 0.35f);

    const Color base_top_left =
        bevel.style == BevelStyle::Raised ? strong_light : strong_dark;
    const Color base_bottom_right =
        bevel.style == BevelStyle::Raised ? strong_dark : strong_light;
    const Color inner_top_left =
        bevel.style == BevelStyle::Raised ? mid_light : mid_dark;
    const Color inner_bottom_right =
        bevel.style == BevelStyle::Raised ? mid_dark : mid_light;

    int layers = std::max(1, static_cast<int>(std::ceil(bevel.thickness)));
    for (int i = 0; i < layers; ++i) {
      bool inner = i > 0;
      Color top_left = inner ? inner_top_left : base_top_left;
      Color bottom_right = inner ? inner_bottom_right : base_bottom_right;

      float inset = static_cast<float>(i);
      float w = draw_rect.width - (inset * 2.0f);
      float h = draw_rect.height - (inset * 2.0f);
      if (w <= 0.0f || h <= 0.0f)
        break;

      RectangleType top = {draw_rect.x + inset, draw_rect.y + inset, w, 1.0f};
      RectangleType left = {draw_rect.x + inset, draw_rect.y + inset, 1.0f, h};
      RectangleType bottom = {draw_rect.x + inset,
                              draw_rect.y + inset + h - 1.0f, w, 1.0f};
      RectangleType right = {draw_rect.x + inset + w - 1.0f,
                             draw_rect.y + inset, 1.0f, h};

      draw_rectangle(top, top_left);
      draw_rectangle(left, top_left);
      draw_rectangle(bottom, bottom_right);
      draw_rectangle(right, bottom_right);
    }
  }

  void render_me(UIContext<InputAction> &context, FontManager &font_manager,
                 Entity &entity) {
    // Defensive check: entity must have UIComponent
    if (!entity.has<UIComponent>())
      return;
    const UIComponent &cmp = entity.get<UIComponent>();
    const float effective_opacity = detail::compute_effective_opacity(entity);
    RectangleType draw_rect = cmp.rect();

    // (debug placeholder)

    // Check if this entity is inside a scroll view (but not the scroll view
    // itself)
    OptEntity scroll_ancestor = detail::find_scroll_view_ancestor(entity);
    // Note: find_clip_ancestor returns entity with HasScrollView OR
    // HasClipChildren Only apply scroll offset if ancestor actually has
    // HasScrollView
    bool inside_scroll_view = scroll_ancestor.valid() &&
                              scroll_ancestor->has<HasScrollView>() &&
                              !entity.has<HasScrollView>();

    // Apply scroll offset to draw_rect if inside a scroll view
    if (inside_scroll_view) {
      Vector2Type scroll_offset =
          scroll_ancestor->get<HasScrollView>().scroll_offset;
      draw_rect.y -= scroll_offset.y;
      draw_rect.x -= scroll_offset.x;
    }

    if (entity.has<HasUIModifiers>()) {
      draw_rect = entity.get<HasUIModifiers>().apply_modifier(draw_rect);
    }

    // Get rotation from modifiers (applied separately since rectangles rotate
    // around center)
    float rotation = entity.has<HasUIModifiers>()
                         ? entity.get<HasUIModifiers>().rotation
                         : 0.0f;

    auto corner_settings = entity.has<HasRoundedCorners>()
                               ? entity.get<HasRoundedCorners>().get()
                               : std::bitset<4>().reset();
    float roundness = entity.has<HasRoundedCorners>()
                          ? entity.get<HasRoundedCorners>().roundness
                          : 0.5f;
    int segments = entity.has<HasRoundedCorners>()
                       ? entity.get<HasRoundedCorners>().segments
                       : 8;

    // Push rotation transform - all subsequent drawing will be rotated around
    // component center
    float centerX = draw_rect.x + draw_rect.width / 2.0f;
    float centerY = draw_rect.y + draw_rect.height / 2.0f;
    push_rotation(centerX, centerY, rotation);

    // Draw shadow first (behind the element)
    render_shadow(entity, draw_rect, corner_settings, effective_opacity,
                  roundness, segments);

    // Draw 9-slice border texture if configured
    // Nine-slice replaces regular background color when present
    if (entity.has<HasNineSliceBorder>()) {
      render_nine_slice(entity, draw_rect, effective_opacity);
    }

    // Focus indicator - draw on entities with FocusClusterRoot when they
    // contain the focused element Check both visual_focus_id match AND
    // alternative check for FocusClusterRoot with focused children
    bool should_draw_focus = (context.visual_focus_id == entity.id);

    // Alternative check: if this entity has FocusClusterRoot and contains the
    // current focus
    if (!should_draw_focus && entity.has<FocusClusterRoot>() &&
        context.focus_id != context.ROOT) {
      // Check if focus_id is this entity or a descendant
      if (context.focus_id == entity.id) {
        should_draw_focus = true;
      } else {
        // Check if focus_id is a child of this entity
        for (EntityID child_id : cmp.children) {
          if (child_id == context.focus_id) {
            should_draw_focus = true;
            break;
          }
          // Also check grandchildren (for components with nested children)
          OptEntity child_opt = UICollectionHolder::getEntityForID(child_id);
          if (child_opt.has_value() && child_opt.asE().has<UIComponent>()) {
            for (EntityID grandchild_id :
                 child_opt.asE().get<UIComponent>().children) {
              if (grandchild_id == context.focus_id) {
                should_draw_focus = true;
                break;
              }
            }
          }
          if (should_draw_focus)
            break;
        }
      }
    }

    if (should_draw_focus) {
      Color focus_col = context.theme.from_usage(Theme::Usage::Focus);
      float effective_focus_opacity = detail::compute_effective_opacity(entity);
      if (effective_focus_opacity < 1.0f) {
        focus_col = colors::opacity_pct(focus_col, effective_focus_opacity);
      }
      // Use theme's focus ring offset to ensure focus ring is outside component
      // bounds
      RectangleType focus_rect =
          cmp.focus_rect(static_cast<int>(context.theme.focus_ring_offset));
      if (entity.has<HasUIModifiers>()) {
        focus_rect = entity.get<HasUIModifiers>().apply_modifier(focus_rect);
      }

      // Respect the entity's corner settings for focus rings
      // If entity has rounded corners, use those; otherwise use theme defaults
      auto focus_corner_settings =
          entity.has<HasRoundedCorners>()
              ? entity.get<HasRoundedCorners>().rounded_corners
              : context.theme.rounded_corners;
      float focus_roundness = entity.has<HasRoundedCorners>()
                                  ? entity.get<HasRoundedCorners>().roundness
                                  : context.theme.roundness;
      int focus_segments = entity.has<HasRoundedCorners>()
                               ? entity.get<HasRoundedCorners>().segments
                               : context.theme.segments;

      // Draw dual-color focus ring for universal contrast on any background
      // Outer contrasting outline (1px outside the focus ring)
      float thickness = context.theme.focus_ring_thickness;
      float lum = colors::luminance(focus_col);
      Color outline_col =
          lum > 0.5f ? Color{0, 0, 0, 180} : Color{255, 255, 255, 180};
      if (effective_focus_opacity < 1.0f) {
        outline_col = colors::opacity_pct(outline_col, effective_focus_opacity);
      }
      {
        RectangleType outlineRect = {focus_rect.x - thickness,
                                     focus_rect.y - thickness,
                                     focus_rect.width + thickness * 2.0f,
                                     focus_rect.height + thickness * 2.0f};
        draw_rectangle_rounded_lines(outlineRect, focus_roundness,
                                     focus_segments, outline_col,
                                     focus_corner_settings);
      }

      // Draw main focus ring with configurable thickness
      for (float t = 0; t < thickness; t += 1.0f) {
        RectangleType thickRect = {focus_rect.x - t, focus_rect.y - t,
                                   focus_rect.width + t * 2.0f,
                                   focus_rect.height + t * 2.0f};
        draw_rectangle_rounded_lines(thickRect, focus_roundness, focus_segments,
                                     focus_col, focus_corner_settings);
      }
    }

    if (entity.has<HasColor>()) {
      Color col = entity.template get<HasColor>().color();

      if (context.is_hot(entity.id) &&
          !entity.template get<HasColor>().skip_hover_override) {
        col = entity.template get<HasColor>().hover_color.value_or(
            context.theme.from_usage(Theme::Usage::Background));
      }

      if (effective_opacity < 1.0f) {
        col = colors::opacity_pct(col, effective_opacity);
      }

      if (col.a > 0) {
        draw_rectangle_rounded(draw_rect, roundness, segments, col,
                               corner_settings);
      }
    }

    render_bevel(entity, draw_rect, effective_opacity);

    // Render circular progress if present (uses ring primitives instead of
    // rectangles)
    render_circular_progress(entity, draw_rect, effective_opacity);

    if (entity.has<HasBorder>()) {
      const Border &border = entity.template get<HasBorder>().border;
      if (border.has_border()) {
        if (border.is_uniform()) {
          Color border_col = border.uniform_color();
          if (effective_opacity < 1.0f) {
            border_col = colors::opacity_pct(border_col, effective_opacity);
          }
          draw_rectangle_rounded_lines(draw_rect, roundness, segments,
                                       border_col, corner_settings);
        } else {
          // Per-side border rendering
          float x = draw_rect.x, y = draw_rect.y;
          float w = draw_rect.width, h = draw_rect.height;
          auto draw_side = [&](const BorderSide &side, float sx, float sy,
                               float sw, float sh) {
            if (!side.has_border())
              return;
            Color c = side.color;
            if (effective_opacity < 1.0f)
              c = colors::opacity_pct(c, effective_opacity);
            draw_rectangle(RectangleType{sx, sy, sw, sh}, c);
          };
          float tt = border.top.thickness.value;
          float bt = border.bottom.thickness.value;
          float lt = border.left.thickness.value;
          float rt = border.right.thickness.value;
          draw_side(border.top, x, y, w, tt);
          draw_side(border.bottom, x, y + h - bt, w, bt);
          draw_side(border.left, x, y + tt, lt, h - tt - bt);
          draw_side(border.right, x + w - rt, y + tt, rt, h - tt - bt);
        }
      }
    }

    if (entity.has<HasLabel>()) {
      const HasLabel &hasLabel = entity.get<HasLabel>();
      Color font_col;

      if (hasLabel.explicit_text_color.has_value()) {
        // Explicit text color set via with_text_color()
        font_col = hasLabel.explicit_text_color.value();
        if (hasLabel.is_disabled) {
          font_col = colors::darken(font_col, 0.5f);
        }
      } else if (hasLabel.background_hint.has_value()) {
        // Garnish auto-contrast: pick best text color for readability
        font_col =
            colors::auto_text_color(hasLabel.background_hint.value(),
                                    context.theme.font, context.theme.darkfont);
        if (hasLabel.is_disabled) {
          font_col = colors::darken(font_col, 0.5f);
        }
      } else {
        // Default: use theme font color (unchanged behavior)
        font_col =
            context.theme.from_usage(Theme::Usage::Font, hasLabel.is_disabled);
      }

      if (effective_opacity < 1.0f) {
        font_col = colors::opacity_pct(font_col, effective_opacity);
      }

      // Prepare text stroke with opacity applied if needed
      std::optional<TextStroke> stroke = hasLabel.text_stroke;
      if (stroke.has_value() && effective_opacity < 1.0f) {
        stroke->color = colors::opacity_pct(stroke->color, effective_opacity);
      }

      // Prepare text shadow with opacity applied if needed
      std::optional<TextShadow> shadow = hasLabel.text_shadow;
      if (shadow.has_value() && effective_opacity < 1.0f) {
        shadow->color = colors::opacity_pct(shadow->color, effective_opacity);
      }

      // Inset text rect when there's a 9-slice border to keep text inside
      RectangleType text_rect = draw_rect;
      if (entity.has<HasNineSliceBorder>()) {
        const NineSliceBorder &ns = entity.get<HasNineSliceBorder>().nine_slice;
        text_rect.x += static_cast<float>(ns.left);
        text_rect.y += static_cast<float>(ns.top);
        text_rect.width -= static_cast<float>(ns.left + ns.right);
        text_rect.height -= static_cast<float>(ns.top + ns.bottom);
      }

      // TODO: unify this font-size resolution with the batched path
      // (see position_text_ex call ~100 lines below) so they don't diverge.
      float explicit_fs = 0.f;
      if (cmp.font_size_explicitly_set) {
        float uis = imm::ThemeDefaults::get().theme.ui_scale;
        explicit_fs = resolve_to_pixels(cmp.font_size, context.screen_height,
                                        cmp.resolved_scaling_mode, uis);
      }

      draw_text_in_rect(font_manager, hasLabel.label.c_str(), text_rect,
                        hasLabel.alignment, font_col, SHOW_TEXT_OVERFLOW_DEBUG,
                        stroke, shadow, rotation, centerX, centerY,
                        hasLabel.text_overflow, hasLabel.letter_spacing,
                        explicit_fs);
    }

    if (entity.has<texture_manager::HasTexture>()) {
      const texture_manager::HasTexture &texture =
          entity.get<texture_manager::HasTexture>();
      // draw textured rect with opacity via color tint
      // NOTE: draw_texture_in_rect path lacks tint, so opacity will apply
      // to images below reuse existing helper (no tint support), so
      // fallback to image path below
      draw_texture_in_rect(texture.texture, draw_rect, texture.alignment);
    } else if (entity.has<ui::HasImage>()) {
      const ui::HasImage &img = entity.get<ui::HasImage>();
      texture_manager::Rectangle src =
          img.source_rect.value_or(texture_manager::Rectangle{
              0.0f, 0.0f, (float)img.texture.width, (float)img.texture.height});

      // Scale to fit height of rect
      float scale = src.height / draw_rect.height;
      Vector2Type size = {src.width / scale, src.height / scale};
      Vector2Type location =
          position_texture(img.texture, size, draw_rect, img.alignment);

      Color img_col = colors::UI_WHITE;
      if (effective_opacity < 1.0f) {
        img_col = colors::opacity_pct(img_col, effective_opacity);
      }
      texture_manager::draw_texture_pro(img.texture, src,
                                        RectangleType{
                                            .x = location.x,
                                            .y = location.y,
                                            .width = size.x,
                                            .height = size.y,
                                        },
                                        size, 0.f, img_col);
    }

    pop_rotation();
  }

  void render(UIContext<InputAction> &context, FontManager &font_manager,
              Entity &entity) {
    // Defensive check: entity must have UIComponent
    if (!entity.has<UIComponent>())
      return;
    const UIComponent &cmp = entity.get<UIComponent>();
    if (cmp.should_hide || entity.has<ShouldHide>())
      return;

    if (cmp.font_name != UIComponent::UNSET_FONT) {
      font_manager.set_active(cmp.font_name);
    }

    // Check if we need scissor clipping for scroll view or clip container
    OptEntity clip_ancestor = detail::find_clip_ancestor(entity);
    bool is_clip_container =
        entity.has<HasScrollView>() || entity.has<HasClipChildren>();
    bool needs_scissor = clip_ancestor.valid() && !is_clip_container;

    // Skip scissor for auto-overflow scroll views where content fits
    if (needs_scissor && clip_ancestor->has<HasScrollView>()) {
      const HasScrollView &sv = clip_ancestor->get<HasScrollView>();
      if (sv.auto_overflow && !sv.needs_scroll_y() && !sv.needs_scroll_x()) {
        needs_scissor = false;
      }
    }

    if (needs_scissor) {
      RectangleType scissor_rect = clip_ancestor->get<UIComponent>().rect();
      begin_scissor_mode(static_cast<int>(scissor_rect.x),
                         static_cast<int>(scissor_rect.y),
                         static_cast<int>(scissor_rect.width),
                         static_cast<int>(scissor_rect.height));
    }

    // Update scroll view content size before rendering (after layout is done)
    if (entity.has<HasScrollView>()) {
      detail::update_scroll_view_content_size(entity);
    }

    if (entity.has<HasColor>() || entity.has<HasLabel>() ||
        entity.has<ui::HasImage>() ||
        entity.has<texture_manager::HasTexture>() ||
        entity.has<FocusClusterRoot>() ||
        entity.has<HasCircularProgressState>() || entity.has<HasScrollView>() ||
        context.visual_focus_id == entity.id) {
      render_me(context, font_manager, entity);
    }

    if (needs_scissor) {
      end_scissor_mode();
    }

    // NOTE: i dont think we need this TODO
    // for (EntityID child : cmp.children) {
    // render(context, font_manager, AutoLayout::to_ent_static(child));
    // }
  }

  virtual void for_each_with_derived(Entity &entity,
                                     UIContext<InputAction> &context,
                                     FontManager &font_manager,
                                     float) override {
#if __WIN32
    // Note we have to do bubble sort here because mingw doesnt support
    // std::ranges::sort
    for (size_t i = 0; i < context.render_cmds.size(); ++i) {
      for (size_t j = i + 1; j < context.render_cmds.size(); ++j) {
        if ((context.render_cmds[i].layer > context.render_cmds[j].layer) ||
            (context.render_cmds[i].layer == context.render_cmds[j].layer &&
             context.render_cmds[i].id > context.render_cmds[j].id)) {
          std::swap(context.render_cmds[i], context.render_cmds[j]);
        }
      }
    }
#else
    std::ranges::sort(context.render_cmds, [](RenderInfo a, RenderInfo b) {
      if (a.layer == b.layer)
        return a.id < b.id;
      return a.layer < b.layer;
    });
#endif

    int cursor_to_set = 0; // Default cursor
    for (auto &cmd : context.render_cmds) {
      auto id = cmd.id;
      OptEntity opt_ent = UICollectionHolder::getEntityForID(id);
      if (!opt_ent.valid())
        continue; // Skip stale entity IDs
      Entity &ent = opt_ent.asE();
      render(context, font_manager, ent);
      if (context.is_hot(ent.id) && ent.has<HasCursor>()) {
        cursor_to_set = to_cursor_id(ent.get<HasCursor>().cursor);
      }
    }
    set_mouse_cursor(cursor_to_set);
    context.render_cmds.clear();
  }
};

// Batched renderer system - collects render commands into a buffer
// then executes them all at once for better batching opportunities
template <typename InputAction>
struct RenderBatched : System<UIContext<InputAction>, FontManager> {
  RenderBatched() : System<UIContext<InputAction>, FontManager>() {
    this->include_derived_children = true;
  }

  void collect_shadow(RenderCommandBuffer &buffer, const Entity &entity,
                      RectangleType draw_rect,
                      const std::bitset<4> &corner_settings,
                      float effective_opacity, int layer,
                      float roundness = 0.5f, int segments = 8) {
    if (!entity.has<HasShadow>())
      return;

    const Shadow &shadow = entity.get<HasShadow>().shadow;
    Color shadow_color = shadow.color;
    if (effective_opacity < 1.0f) {
      shadow_color.a =
          static_cast<unsigned char>(shadow_color.a * effective_opacity);
    }

    if (shadow.style == ShadowStyle::Hard) {
      RectangleType shadow_rect = {draw_rect.x + shadow.offset_x,
                                   draw_rect.y + shadow.offset_y,
                                   draw_rect.width, draw_rect.height};
      if (corner_settings.any()) {
        buffer.add_rounded_rectangle(shadow_rect, shadow_color, roundness,
                                     segments, corner_settings, layer,
                                     entity.id);
      } else {
        buffer.add_rectangle(shadow_rect, shadow_color, layer, entity.id);
      }
    } else {
      int layers = static_cast<int>(shadow.blur_radius / 2.0f);
      layers = std::max(3, std::min(layers, 8));

      for (int i = layers; i >= 0; --i) {
        float spread = shadow.blur_radius *
                       (static_cast<float>(i) / static_cast<float>(layers));
        float alpha_factor =
            1.0f - (static_cast<float>(i) / static_cast<float>(layers + 1));

        RectangleType shadow_rect = {
            draw_rect.x + shadow.offset_x - spread * 0.5f,
            draw_rect.y + shadow.offset_y - spread * 0.5f,
            draw_rect.width + spread, draw_rect.height + spread};

        Color layer_color = shadow_color;
        layer_color.a = static_cast<unsigned char>(
            static_cast<float>(shadow_color.a) * alpha_factor *
            (1.0f / static_cast<float>(layers)));

        if (corner_settings.any()) {
          buffer.add_rounded_rectangle(shadow_rect, layer_color, roundness,
                                       segments, corner_settings, layer,
                                       entity.id);
        } else {
          buffer.add_rectangle(shadow_rect, layer_color, layer, entity.id);
        }
      }
    }
  }

  void collect_nine_slice(RenderCommandBuffer &buffer, const Entity &entity,
                          RectangleType draw_rect, float effective_opacity,
                          int layer) {
    if (!entity.has<HasNineSliceBorder>())
      return;

    const NineSliceBorder &nine_slice =
        entity.get<HasNineSliceBorder>().nine_slice;
    Color tint = nine_slice.tint;
    if (effective_opacity < 1.0f) {
      tint.a = static_cast<unsigned char>(tint.a * effective_opacity);
    }

    buffer.add_nine_slice(draw_rect, nine_slice.texture, nine_slice.left,
                          nine_slice.top, nine_slice.right, nine_slice.bottom,
                          tint, layer, entity.id);
  }

  void collect_circular_progress(RenderCommandBuffer &buffer,
                                 const Entity &entity, RectangleType draw_rect,
                                 float effective_opacity, int layer) {
    if (!entity.has<HasCircularProgressState>()) {
      return;
    }

    const HasCircularProgressState &state =
        entity.get<HasCircularProgressState>();

    float centerX = draw_rect.x + draw_rect.width / 2.0f;
    float centerY = draw_rect.y + draw_rect.height / 2.0f;
    float outerRadius = std::min(draw_rect.width, draw_rect.height) / 2.0f;
    float innerRadius = outerRadius - state.thickness;
    if (innerRadius < 0.0f)
      innerRadius = 0.0f;

    Color track_color = state.track_color;
    Color fill_color = state.fill_color;
    if (effective_opacity < 1.0f) {
      track_color = colors::opacity_pct(track_color, effective_opacity);
      fill_color = colors::opacity_pct(fill_color, effective_opacity);
    }

    int segments = std::max(32, static_cast<int>(outerRadius * 0.5f));

    // Background track
    buffer.add_ring(centerX, centerY, innerRadius, outerRadius, segments,
                    track_color, layer, entity.id);

    // Progress fill
    if (state.value > 0.001f) {
      float end_angle = state.start_angle + (state.value * 360.0f);
      buffer.add_ring_segment(centerX, centerY, innerRadius, outerRadius,
                              state.start_angle, end_angle, segments,
                              fill_color, layer, entity.id);
    }
  }

  void collect_bevel(RenderCommandBuffer &buffer, const Entity &entity,
                     RectangleType draw_rect, float effective_opacity,
                     int layer) {
    if (!entity.has<HasBevelBorder>())
      return;

    const BevelBorder &bevel = entity.get<HasBevelBorder>().bevel;
    if (!bevel.has_bevel())
      return;

    Color light = bevel.light_color;
    Color dark = bevel.dark_color;
    if (effective_opacity < 1.0f) {
      light = colors::opacity_pct(light, effective_opacity);
      dark = colors::opacity_pct(dark, effective_opacity);
    }

    Color base_fill = colors::UI_WHITE;
    if (entity.has<HasColor>()) {
      base_fill = entity.get<HasColor>().color();
    }
    if (effective_opacity < 1.0f) {
      base_fill = colors::opacity_pct(base_fill, effective_opacity);
    }

    const Color strong_light = light;
    const Color strong_dark = dark;
    const Color mid_light = colors::lighten(base_fill, 0.35f);
    const Color mid_dark = colors::darken(base_fill, 0.35f);

    const Color base_top_left =
        bevel.style == BevelStyle::Raised ? strong_light : strong_dark;
    const Color base_bottom_right =
        bevel.style == BevelStyle::Raised ? strong_dark : strong_light;
    const Color inner_top_left =
        bevel.style == BevelStyle::Raised ? mid_light : mid_dark;
    const Color inner_bottom_right =
        bevel.style == BevelStyle::Raised ? mid_dark : mid_light;

    int layers = std::max(1, static_cast<int>(std::ceil(bevel.thickness)));
    for (int i = 0; i < layers; ++i) {
      bool inner = i > 0;
      Color top_left = inner ? inner_top_left : base_top_left;
      Color bottom_right = inner ? inner_bottom_right : base_bottom_right;

      float inset = static_cast<float>(i);
      float w = draw_rect.width - (inset * 2.0f);
      float h = draw_rect.height - (inset * 2.0f);
      if (w <= 0.0f || h <= 0.0f)
        break;

      RectangleType top = {draw_rect.x + inset, draw_rect.y + inset, w, 1.0f};
      RectangleType left = {draw_rect.x + inset, draw_rect.y + inset, 1.0f, h};
      RectangleType bottom = {draw_rect.x + inset,
                              draw_rect.y + inset + h - 1.0f, w, 1.0f};
      RectangleType right = {draw_rect.x + inset + w - 1.0f,
                             draw_rect.y + inset, 1.0f, h};

      buffer.add_rectangle(top, top_left, layer, entity.id);
      buffer.add_rectangle(left, top_left, layer, entity.id);
      buffer.add_rectangle(bottom, bottom_right, layer, entity.id);
      buffer.add_rectangle(right, bottom_right, layer, entity.id);
    }
  }

  void collect_me(RenderCommandBuffer &buffer, UIContext<InputAction> &context,
                  FontManager &font_manager, Entity &entity, int layer) {
    // Defensive check: entity must have UIComponent
    if (!entity.has<UIComponent>())
      return;
    const UIComponent &cmp = entity.get<UIComponent>();

    const float effective_opacity = detail::compute_effective_opacity(entity);
    RectangleType draw_rect = cmp.rect();

    OptEntity scroll_ancestor = detail::find_scroll_view_ancestor(entity);
    // Note: find_clip_ancestor returns entity with HasScrollView OR
    // HasClipChildren Only apply scroll offset if ancestor actually has
    // HasScrollView
    bool inside_scroll_view = scroll_ancestor.valid() &&
                              scroll_ancestor->has<HasScrollView>() &&
                              !entity.has<HasScrollView>();

    if (inside_scroll_view) {
      Vector2Type scroll_offset =
          scroll_ancestor->get<HasScrollView>().scroll_offset;
      draw_rect.y -= scroll_offset.y;
      draw_rect.x -= scroll_offset.x;
    }

    if (entity.has<HasUIModifiers>()) {
      draw_rect = entity.get<HasUIModifiers>().apply_modifier(draw_rect);
    }

    // Get rotation from modifiers (applied separately since rectangles rotate
    // around center)
    float rotation = entity.has<HasUIModifiers>()
                         ? entity.get<HasUIModifiers>().rotation
                         : 0.0f;

    auto corner_settings = entity.has<HasRoundedCorners>()
                               ? entity.get<HasRoundedCorners>().get()
                               : std::bitset<4>().reset();
    float roundness = entity.has<HasRoundedCorners>()
                          ? entity.get<HasRoundedCorners>().roundness
                          : 0.5f;
    int segments = entity.has<HasRoundedCorners>()
                       ? entity.get<HasRoundedCorners>().segments
                       : 8;

    // Shadow first
    collect_shadow(buffer, entity, draw_rect, corner_settings,
                   effective_opacity, layer, roundness, segments);

    // Nine-slice border
    if (entity.has<HasNineSliceBorder>()) {
      collect_nine_slice(buffer, entity, draw_rect, effective_opacity, layer);
    }

    // Focus indicator - draw on entities with FocusClusterRoot when they
    // contain the focused element Check both visual_focus_id match AND
    // alternative check for FocusClusterRoot with focused children
    bool should_draw_focus = (context.visual_focus_id == entity.id);

    // Alternative check: if this entity has FocusClusterRoot and contains the
    // current focus
    if (!should_draw_focus && entity.has<FocusClusterRoot>() &&
        context.focus_id != context.ROOT) {
      // Check if focus_id is this entity or a descendant
      if (context.focus_id == entity.id) {
        should_draw_focus = true;
      } else {
        // Check if focus_id is a child of this entity
        for (EntityID child_id : cmp.children) {
          if (child_id == context.focus_id) {
            should_draw_focus = true;
            break;
          }
          // Also check grandchildren (for components with nested children)
          OptEntity child_opt = UICollectionHolder::getEntityForID(child_id);
          if (child_opt.has_value() && child_opt.asE().has<UIComponent>()) {
            for (EntityID grandchild_id :
                 child_opt.asE().get<UIComponent>().children) {
              if (grandchild_id == context.focus_id) {
                should_draw_focus = true;
                break;
              }
            }
          }
          if (should_draw_focus)
            break;
        }
      }
    }

    if (should_draw_focus) {
      Color focus_col = context.theme.from_usage(Theme::Usage::Focus);
      float effective_focus_opacity = detail::compute_effective_opacity(entity);
      if (effective_focus_opacity < 1.0f) {
        focus_col = colors::opacity_pct(focus_col, effective_focus_opacity);
      }
      // Use theme's focus ring offset to ensure focus ring is outside component
      // bounds
      RectangleType focus_rect =
          cmp.focus_rect(static_cast<int>(context.theme.focus_ring_offset));
      if (entity.has<HasUIModifiers>()) {
        focus_rect = entity.get<HasUIModifiers>().apply_modifier(focus_rect);
      }

      // Respect the entity's corner settings for focus rings
      // If entity has rounded corners, use those; otherwise use theme defaults
      auto focus_corner_settings =
          entity.has<HasRoundedCorners>()
              ? entity.get<HasRoundedCorners>().rounded_corners
              : context.theme.rounded_corners;
      float focus_roundness = entity.has<HasRoundedCorners>()
                                  ? entity.get<HasRoundedCorners>().roundness
                                  : context.theme.roundness;
      int focus_segments = entity.has<HasRoundedCorners>()
                               ? entity.get<HasRoundedCorners>().segments
                               : context.theme.segments;

      // Dual-color focus ring: contrasting outline for universal visibility
      float thickness = context.theme.focus_ring_thickness;
      float lum = colors::luminance(focus_col);
      Color outline_col =
          lum > 0.5f ? Color{0, 0, 0, 180} : Color{255, 255, 255, 180};
      if (effective_focus_opacity < 1.0f) {
        outline_col = colors::opacity_pct(outline_col, effective_focus_opacity);
      }
      RectangleType outline_rect = {focus_rect.x - thickness,
                                    focus_rect.y - thickness,
                                    focus_rect.width + thickness * 2.0f,
                                    focus_rect.height + thickness * 2.0f};
      buffer.add_rounded_rectangle_outline(
          outline_rect, outline_col, focus_roundness, focus_segments,
          focus_corner_settings, layer + 199, entity.id);

      // Main focus ring
      buffer.add_rounded_rectangle_outline(
          focus_rect, focus_col, focus_roundness, focus_segments,
          focus_corner_settings, layer + 200, entity.id,
          context.theme.focus_ring_thickness);
    }

    // Background color
    if (entity.has<HasColor>()) {
      Color col = entity.template get<HasColor>().color();

      if (context.is_hot(entity.id) &&
          !entity.template get<HasColor>().skip_hover_override) {
        col = entity.template get<HasColor>().hover_color.value_or(
            context.theme.from_usage(Theme::Usage::Background));
      }

      if (effective_opacity < 1.0f) {
        col = colors::opacity_pct(col, effective_opacity);
      }

      if (col.a > 0) {
        buffer.add_rounded_rectangle(draw_rect, col, roundness, segments,
                                     corner_settings, layer, entity.id,
                                     rotation);
      }
    }

    // Bevel border
    collect_bevel(buffer, entity, draw_rect, effective_opacity, layer);

    // Circular progress
    collect_circular_progress(buffer, entity, draw_rect, effective_opacity,
                              layer);

    // Border
    if (entity.has<HasBorder>()) {
      const Border &border = entity.template get<HasBorder>().border;
      if (border.has_border()) {
        if (border.is_uniform()) {
          Color border_col = border.uniform_color();
          if (effective_opacity < 1.0f) {
            border_col = colors::opacity_pct(border_col, effective_opacity);
          }
          buffer.add_rounded_rectangle_outline(draw_rect, border_col, roundness,
                                               segments, corner_settings, layer,
                                               entity.id);
        } else {
          // Per-side border rendering (as filled rectangles)
          float x = draw_rect.x, y = draw_rect.y;
          float w = draw_rect.width, h = draw_rect.height;
          auto add_side = [&](const BorderSide &side, float sx, float sy,
                              float sw, float sh) {
            if (!side.has_border())
              return;
            Color c = side.color;
            if (effective_opacity < 1.0f)
              c = colors::opacity_pct(c, effective_opacity);
            RectangleType side_rect{sx, sy, sw, sh};
            buffer.add_rounded_rectangle(side_rect, c, 0.f, 1, corner_settings,
                                         layer, entity.id, 0.f);
          };
          float tt = border.top.thickness.value;
          float bt = border.bottom.thickness.value;
          float lt = border.left.thickness.value;
          float rt = border.right.thickness.value;
          add_side(border.top, x, y, w, tt);
          add_side(border.bottom, x, y + h - bt, w, bt);
          add_side(border.left, x, y + tt, lt, h - tt - bt);
          add_side(border.right, x + w - rt, y + tt, rt, h - tt - bt);
        }
      }
    }

    // Label/text
    if (entity.has<HasLabel>()) {
      const HasLabel &hasLabel = entity.get<HasLabel>();
      Color font_col;

      if (hasLabel.explicit_text_color.has_value()) {
        font_col = hasLabel.explicit_text_color.value();
        if (hasLabel.is_disabled) {
          font_col = colors::darken(font_col, 0.5f);
        }
      } else if (hasLabel.background_hint.has_value()) {
        font_col =
            colors::auto_text_color(hasLabel.background_hint.value(),
                                    context.theme.font, context.theme.darkfont);
        if (hasLabel.is_disabled) {
          font_col = colors::darken(font_col, 0.5f);
        }
      } else {
        font_col =
            context.theme.from_usage(Theme::Usage::Font, hasLabel.is_disabled);
      }

      if (effective_opacity < 1.0f) {
        font_col = colors::opacity_pct(font_col, effective_opacity);
      }

      std::optional<TextStroke> stroke = hasLabel.text_stroke;
      if (stroke.has_value() && effective_opacity < 1.0f) {
        stroke->color = colors::opacity_pct(stroke->color, effective_opacity);
      }

      std::optional<TextShadow> shadow = hasLabel.text_shadow;
      if (shadow.has_value() && effective_opacity < 1.0f) {
        shadow->color = colors::opacity_pct(shadow->color, effective_opacity);
      }

      RectangleType text_rect = draw_rect;
      if (entity.has<HasNineSliceBorder>()) {
        const NineSliceBorder &ns = entity.get<HasNineSliceBorder>().nine_slice;
        text_rect.x += static_cast<float>(ns.left);
        text_rect.y += static_cast<float>(ns.top);
        text_rect.width -= static_cast<float>(ns.left + ns.right);
        text_rect.height -= static_cast<float>(ns.top + ns.bottom);
      }

      // When a font size was explicitly set (via with_font / with_font_size),
      // use it as an upper bound so text doesn't auto-grow beyond that size.
      // Default font sizes (from UIStylingDefaults) are NOT applied as caps
      // to preserve the existing auto-fit-to-container behavior.
      float explicit_fs = 0.f;
      if (cmp.font_size_explicitly_set) {
        // Use scaling-mode-aware resolution so font pixels scale with
        // ui_scale in Adaptive mode.
        float uis = imm::ThemeDefaults::get().theme.ui_scale;
        explicit_fs = resolve_to_pixels(cmp.font_size, context.screen_height,
                                        cmp.resolved_scaling_mode, uis);
      }

      // Position text to get font size
      TextPositionResult result = position_text_ex(
          font_manager, hasLabel.label.c_str(), text_rect, hasLabel.alignment,
          Vector2Type{5.f, 5.f}, explicit_fs, hasLabel.letter_spacing,
          hasLabel.text_overflow);

      if (result.rect.height >= MIN_FONT_SIZE) {
        // Handle text overflow ellipsis truncation for batched path
        std::string display_text = hasLabel.label;
        if (hasLabel.text_overflow == TextOverflow::Ellipsis &&
            !hasLabel.label.empty()) {
          Font font = font_manager.get_active_font();
          float font_size = result.rect.height;
          float spacing = 1.f + hasLabel.letter_spacing;
          float max_width = text_rect.width - 10.f;
          if (max_width > 0.f) {
            Vector2Type ts =
                measure_text(font, hasLabel.label.c_str(), font_size, spacing);
            if (ts.x > max_width) {
              const std::string ellipsis = "...";
              Vector2Type es =
                  measure_text(font, ellipsis.c_str(), font_size, spacing);
              float available = max_width - es.x;
              if (available <= 0.f) {
                display_text = ellipsis;
              } else {
                size_t low = 0, high = hasLabel.label.size(), best = 0;
                while (low <= high && high <= hasLabel.label.size()) {
                  size_t mid = (low + high) / 2;
                  std::string prefix = hasLabel.label.substr(0, mid);
                  Vector2Type ps =
                      measure_text(font, prefix.c_str(), font_size, spacing);
                  if (ps.x <= available) {
                    best = mid;
                    low = mid + 1;
                  } else {
                    if (mid == 0)
                      break;
                    high = mid - 1;
                  }
                }
                display_text = hasLabel.label.substr(0, best) + ellipsis;
              }
            }
          }
        }

        // Pass the container rect (text_rect) not the position rect
        // (result.rect) render_text will handle centering within the container
        float centerX = draw_rect.x + draw_rect.width / 2.0f;
        float centerY = draw_rect.y + draw_rect.height / 2.0f;
        buffer.add_text(text_rect, display_text, cmp.font_name,
                        result.rect.height, font_col, hasLabel.alignment, layer,
                        entity.id, stroke, shadow, rotation, centerX, centerY,
                        hasLabel.letter_spacing);

#ifdef AFTER_HOURS_ENABLE_E2E_TESTING
        // Register text for E2E testing (only visible-in-viewport text)
        if (testing::test_input::detail::test_mode) {
          testing::VisibleTextRegistry::instance().register_text_if_visible(
              hasLabel.label, draw_rect.x, draw_rect.y, draw_rect.width,
              draw_rect.height, context.screen_width, context.screen_height);
        }
#endif
      }
    }

    // Texture
    if (entity.has<texture_manager::HasTexture>()) {
      const texture_manager::HasTexture &texture =
          entity.get<texture_manager::HasTexture>();
      float scale = (float)texture.texture.height / draw_rect.height;
      Vector2Type size = {
          (float)texture.texture.width / scale,
          (float)texture.texture.height / scale,
      };
      Vector2Type location =
          position_texture(texture.texture, size, draw_rect, texture.alignment);
      RectangleType dest = {location.x, location.y, size.x, size.y};
      RectangleType src = {0.0f, 0.0f, (float)texture.texture.width,
                           (float)texture.texture.height};
      buffer.add_image(dest, src, texture.texture, colors::UI_WHITE, layer,
                       entity.id);
    } else if (entity.has<ui::HasImage>()) {
      const ui::HasImage &img = entity.get<ui::HasImage>();
      texture_manager::Rectangle src =
          img.source_rect.value_or(texture_manager::Rectangle{
              0.0f, 0.0f, (float)img.texture.width, (float)img.texture.height});

      float scale = src.height / draw_rect.height;
      Vector2Type size = {src.width / scale, src.height / scale};
      Vector2Type location =
          position_texture(img.texture, size, draw_rect, img.alignment);

      Color img_col = colors::UI_WHITE;
      if (effective_opacity < 1.0f) {
        img_col = colors::opacity_pct(img_col, effective_opacity);
      }

      RectangleType dest = {location.x, location.y, size.x, size.y};
      buffer.add_image(dest, src, img.texture, img_col, layer, entity.id);
    }
  }

  void collect(RenderCommandBuffer &buffer, UIContext<InputAction> &context,
               FontManager &font_manager, Entity &entity, int layer) {
    // Defensive check: entity must have UIComponent to be rendered
    if (!entity.has<UIComponent>())
      return;
    const UIComponent &cmp = entity.get<UIComponent>();
    if (cmp.should_hide || entity.has<ShouldHide>())
      return;

    if (cmp.font_name != UIComponent::UNSET_FONT) {
      font_manager.set_active(cmp.font_name);
    }

    // Check if we need scissor clipping for scroll view or clip container
    OptEntity clip_ancestor = detail::find_clip_ancestor(entity);
    bool is_clip_container =
        entity.has<HasScrollView>() || entity.has<HasClipChildren>();
    bool needs_scissor = clip_ancestor.valid() && !is_clip_container;

    // Skip scissor for auto-overflow scroll views where content fits
    if (needs_scissor && clip_ancestor->has<HasScrollView>()) {
      const HasScrollView &sv = clip_ancestor->get<HasScrollView>();
      if (sv.auto_overflow && !sv.needs_scroll_y() && !sv.needs_scroll_x()) {
        needs_scissor = false;
      }
    }

    if (needs_scissor) {
      RectangleType scissor_rect = clip_ancestor->get<UIComponent>().rect();
      buffer.add_scissor_start(
          static_cast<int>(scissor_rect.x), static_cast<int>(scissor_rect.y),
          static_cast<int>(scissor_rect.width),
          static_cast<int>(scissor_rect.height), layer, entity.id);
    }

    // Update scroll view content size
    if (entity.has<HasScrollView>()) {
      detail::update_scroll_view_content_size(entity);
    }

    if (entity.has<HasColor>() || entity.has<HasLabel>() ||
        entity.has<ui::HasImage>() ||
        entity.has<texture_manager::HasTexture>() ||
        entity.has<FocusClusterRoot>() ||
        entity.has<HasCircularProgressState>() || entity.has<HasScrollView>() ||
        context.visual_focus_id == entity.id) {
      collect_me(buffer, context, font_manager, entity, layer);
    }

    if (needs_scissor) {
      buffer.add_scissor_end(layer, entity.id);
    }
  }

  virtual void for_each_with_derived(Entity &, UIContext<InputAction> &context,
                                     FontManager &font_manager,
                                     float) override {
    // Reset arena for new frame
    Arena &arena = get_render_arena();
    arena.reset();

    // Create command buffer
    RenderCommandBuffer buffer(arena);

    // Sort render commands by layer
#if __WIN32
    for (size_t i = 0; i < context.render_cmds.size(); ++i) {
      for (size_t j = i + 1; j < context.render_cmds.size(); ++j) {
        if ((context.render_cmds[i].layer > context.render_cmds[j].layer) ||
            (context.render_cmds[i].layer == context.render_cmds[j].layer &&
             context.render_cmds[i].id > context.render_cmds[j].id)) {
          std::swap(context.render_cmds[i], context.render_cmds[j]);
        }
      }
    }
#else
    std::ranges::sort(context.render_cmds, [](RenderInfo a, RenderInfo b) {
      if (a.layer == b.layer)
        return a.id < b.id;
      return a.layer < b.layer;
    });
#endif

    // Collect all commands
    int cursor_to_set = 0; // Default cursor
    for (const auto &cmd : context.render_cmds) {
      auto id = cmd.id;
      auto layer = cmd.layer;
      OptEntity opt_ent = UICollectionHolder::getEntityForID(id);
      if (!opt_ent.valid())
        continue; // Skip stale entity IDs
      Entity &ent = opt_ent.asE();
      collect(buffer, context, font_manager, ent, layer);
      if (context.is_hot(ent.id) && ent.has<HasCursor>()) {
        cursor_to_set = to_cursor_id(ent.get<HasCursor>().cursor);
      }
    }
    set_mouse_cursor(cursor_to_set);
    context.render_cmds.clear();

    // Execute all commands with batching
    BatchedRenderer renderer;
    renderer.render(buffer, font_manager);
  }
};

} // namespace ui

} // namespace afterhours
