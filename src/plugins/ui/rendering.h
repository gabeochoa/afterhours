#pragma once

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

#include "../../drawing_helpers.h"
#include "../../ecs.h"
#include "../../font_helper.h"
#include "../../logging.h"
#include "../animation.h"
#include "../input_system.h"
#include "../texture_manager.h"
#include "../window_manager.h"
#include "animation_keys.h"
#include "components.h"
#include "context.h"
#include "fmt/format.h"
#include "systems.h"
#include "ui_modifiers.h"
#include "text_utils.h"
#include "theme.h"

namespace afterhours {

namespace ui {

static inline float _compute_effective_opacity(const Entity &entity) {
  float result = 1.0f;
  const Entity *cur = &entity;
  int guard = 0;
  while (cur) {
    if (cur->has<HasOpacity>()) {
      result *= std::clamp(cur->get<HasOpacity>().value, 0.0f, 1.0f);
    }
    if (!cur->has<UIComponent>())
      break;
    EntityID pid = cur->get<UIComponent>().parent;
    if (pid < 0 || pid == cur->id)
      break;
    cur = &EntityHelper::getEntityForIDEnforce(pid);
    if (++guard > 64)
      break;
  }
  return std::clamp(result, 0.0f, 1.0f);
}
// Minimum font size to prevent invalid rendering (font size 0)
// This ensures text always attempts to render, even if too small to read
constexpr float MIN_FONT_SIZE = 1.0f;
// Font size threshold for debug visualization - text is likely unreadable
constexpr float DEBUG_FONT_SIZE_THRESHOLD = 8.0f;

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

static inline TextPositionResult position_text_ex(const ui::FontManager &fm,
                                                  const std::string &text,
                                                  RectangleType container,
                                                  TextAlignment alignment,
                                                  Vector2Type margin_px) {
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

  // Calculate the maximum text size based on the container size and margins
  Vector2Type max_text_size = Vector2Type{
      .x = container.width - 2 * margin_px.x,
      .y = container.height - 2 * margin_px.y,
  };

  // Check for invalid container (negative or zero usable space)
  if (max_text_size.x <= 0 || max_text_size.y <= 0) {
    log_warn("Container too small for text: container={}x{}, margins={}x{}, "
             "text='{}'",
             container.width, container.height, margin_px.x, margin_px.y,
             text.length() > 20 ? text.substr(0, 20) + "..." : text);
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

  // Use binary search to find largest font size that fits
  float low = MIN_FONT_SIZE;
  float high = std::min(max_text_size.y, 200.f); // Cap at reasonable max
  float font_size = low;
  Vector2Type text_size;

  while (high - low > 0.5f) {
    float mid = (low + high) / 2.f;
    text_size = measure_text(font, text.c_str(), mid, 1.f);
    if (text_size.x <= max_text_size.x && text_size.y <= max_text_size.y) {
      font_size = mid;
      low = mid;
    } else {
      high = mid;
    }
  }

  // Clamp to minimum font size to prevent invalid rendering
  bool text_fits = font_size >= MIN_FONT_SIZE;
  if (font_size < MIN_FONT_SIZE) {
    log_warn("Text '{}' cannot fit in container {}x{} with margins {}x{} - "
             "clamping font size from {} to {}",
             text.length() > 20 ? text.substr(0, 20) + "..." : text,
             container.width, container.height, margin_px.x, margin_px.y,
             font_size, MIN_FONT_SIZE);
    font_size = MIN_FONT_SIZE;
  }

  // Re-measure with final clamped font size for accurate positioning
  text_size = measure_text(font, text.c_str(), font_size, 1.f);

  // Calculate the text position based on the alignment and margins
  Vector2Type position;
  switch (alignment) {
  case TextAlignment::Left:
    position = Vector2Type{
        .x = container.x + margin_px.x,
        .y = container.y + margin_px.y +
             (container.height - 2 * margin_px.y - text_size.y) / 2,
    };
    break;
  case TextAlignment::Center:
    position = Vector2Type{
        .x = container.x + margin_px.x +
             (container.width - 2 * margin_px.x - text_size.x) / 2,
        .y = container.y + margin_px.y +
             (container.height - 2 * margin_px.y - text_size.y) / 2,
    };
    break;
  case TextAlignment::Right:
    position = Vector2Type{
        .x = container.x + container.width - margin_px.x - text_size.x,
        .y = container.y + margin_px.y +
             (container.height - 2 * margin_px.y - text_size.y) / 2,
    };
    break;
  default:
    // Handle unknown alignment (shouldn't happen)
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
static inline void _draw_text_at_position(const ui::FontManager &fm,
                                          const std::string &text,
                                          RectangleType rect,
                                          TextAlignment alignment,
                                          RectangleType sizing, Color color) {
  // Always use UTF-8 aware rendering (works for all text including CJK)
  Font font = fm.get_active_font();
  float fontSize = sizing.height;
  float spacing = 1.0f;

  // Calculate offset between sizing and original rect (for shadow/stroke)
  float offset_x = sizing.x - rect.x;
  float offset_y = sizing.y - rect.y;

  Vector2Type startPos = {sizing.x, sizing.y};
  if (alignment == TextAlignment::Center) {
    Vector2Type textSize =
        measure_text_utf8(font, text.c_str(), fontSize, spacing);
    // Apply centering within rect, THEN add the offset
    startPos.x = rect.x + (rect.width - textSize.x) / 2.0f + offset_x;
    startPos.y = rect.y + (rect.height - textSize.y) / 2.0f + offset_y;
  }
  draw_text_ex(font, text.c_str(), startPos, fontSize, spacing, color);
}

static inline void
draw_text_in_rect(const ui::FontManager &fm, const std::string &text,
                  RectangleType rect, TextAlignment alignment, Color color,
                  bool show_debug_indicator = false,
                  const std::optional<TextStroke> &stroke = std::nullopt,
                  const std::optional<TextShadow> &shadow = std::nullopt) {
  TextPositionResult result =
      position_text_ex(fm, text, rect, alignment, Vector2Type{5.f, 5.f});

  // Draw visual debug indicator if text doesn't fit and debug is enabled
  // The indicator is a small red triangle in the corner of the container
  if (show_debug_indicator && !result.text_fits) {
    // Draw a small warning indicator (red corner triangle)
    Color warning_color = Color{255, 50, 50, 200}; // Semi-transparent red
    float indicator_size = std::min(8.0f, std::min(rect.width, rect.height));
    // Draw as a small rectangle in top-right corner
    draw_rectangle(
        RectangleType{
            .x = rect.x + rect.width - indicator_size,
            .y = rect.y,
            .width = indicator_size,
            .height = indicator_size,
        },
        warning_color);
  }

  // Don't attempt to render if font size is effectively zero
  if (result.rect.height < MIN_FONT_SIZE) {
    return;
  }

  RectangleType sizing = result.rect;

  // Draw text shadow first (behind everything)
  // Renders text at a single offset position to create a drop shadow effect
  if (shadow.has_value() && shadow->has_shadow()) {
    RectangleType shadow_sizing = sizing;
    shadow_sizing.x += shadow->offset_x;
    shadow_sizing.y += shadow->offset_y;
    _draw_text_at_position(fm, text, rect, alignment, shadow_sizing,
                           shadow->color);
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
      _draw_text_at_position(fm, text, rect, alignment, offset_sizing,
                             stroke_color);
    }
  }

  // Draw main text on top
  _draw_text_at_position(fm, text, rect, alignment, sizing, color);
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

  mutable UIContext<InputAction> *context;

  mutable int level = 0;
  mutable int indent = 0;
  mutable EntityID isolated_id = -1;
  mutable bool isolate_enabled = false;
  enum struct IsolationMode { NodeOnly, NodeAndDescendants };
  mutable IsolationMode isolation_mode = IsolationMode::NodeOnly;

  float fontSize = 20.0f;

  RenderDebugAutoLayoutRoots(InputAction toggle_kp) : toggle_action(toggle_kp) {
    this->include_derived_children = true;
  }

  virtual ~RenderDebugAutoLayoutRoots() {}

  virtual bool should_run(float dt) override {
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

  virtual void once(float) const override {
    this->context =
        EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();

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
    const Entity *cur = &entity;
    int guard = 0;
    while (cur->has<UIComponent>()) {
      const UIComponent &cur_cmp = cur->get<UIComponent>();
      if (cur_cmp.parent < 0 || ++guard > 64)
        break;
      if (cur_cmp.parent == isolated_id)
        return true;
      cur = &EntityHelper::getEntityForIDEnforce(cur_cmp.parent);
    }
    return false;
  }

  void render_me(const Entity &entity) const {
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

  void render(const Entity &entity) const {
    const UIComponent &cmp = entity.get<UIComponent>();
    if (cmp.should_hide)
      return;

    if (cmp.was_rendered_to_screen) {
      render_me(entity);
      level++;
    }

    indent++;
    for (EntityID child : cmp.children) {
      render(AutoLayout::to_ent_static(child));
    }
    indent--;
  }

  virtual void for_each_with_derived(const Entity &entity, const UIComponent &,
                                     const AutoLayoutRoot &, float) const {
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
                     int segments = 8) const {
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

  void render_bevel(const Entity &entity, RectangleType draw_rect,
                    float effective_opacity) const {
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

  void render_me(const UIContext<InputAction> &context,
                 const FontManager &font_manager, const Entity &entity) const {
    const UIComponent &cmp = entity.get<UIComponent>();
    const float effective_opacity = _compute_effective_opacity(entity);
    RectangleType draw_rect = cmp.rect();

    draw_rect = apply_ui_modifiers_recursive(entity.id, draw_rect);

    auto corner_settings = entity.has<HasRoundedCorners>()
                               ? entity.get<HasRoundedCorners>().get()
                               : std::bitset<4>().reset();
    float roundness = entity.has<HasRoundedCorners>()
                          ? entity.get<HasRoundedCorners>().roundness
                          : 0.5f;
    int segments = entity.has<HasRoundedCorners>()
                       ? entity.get<HasRoundedCorners>().segments
                       : 8;

    // Draw shadow first (behind the element)
    render_shadow(entity, draw_rect, corner_settings, effective_opacity,
                  roundness, segments);

    if (context.visual_focus_id == entity.id) {
      Color focus_col = context.theme.from_usage(Theme::Usage::Accent);
      float effective_focus_opacity = _compute_effective_opacity(entity);
      if (effective_focus_opacity < 1.0f) {
        focus_col = colors::opacity_pct(focus_col, effective_focus_opacity);
      }
      RectangleType focus_rect = cmp.focus_rect();
      focus_rect = apply_ui_modifiers_recursive(entity.id, focus_rect);
      if (corner_settings.any()) {
        draw_rectangle_rounded(focus_rect, roundness, segments, focus_col,
                               corner_settings);
      } else {
        draw_rectangle_outline(focus_rect, focus_col);
      }
    }

    if (entity.has<HasColor>()) {
      Color col = entity.template get<HasColor>().color();

      if (context.is_hot(entity.id)) {
        col = context.theme.from_usage(Theme::Usage::Background);
      }

      if (effective_opacity < 1.0f) {
        col = colors::opacity_pct(col, effective_opacity);
      }

      draw_rectangle_rounded(draw_rect, roundness, segments, col,
                             corner_settings);
    }

    render_bevel(entity, draw_rect, effective_opacity);

    if (entity.has<HasBorder>()) {
      const Border &border = entity.template get<HasBorder>().border;
      if (border.has_border()) {
        Color border_col = border.color;
        if (effective_opacity < 1.0f) {
          border_col = colors::opacity_pct(border_col, effective_opacity);
        }
        draw_rectangle_rounded_lines(draw_rect, roundness, segments, border_col,
                                     corner_settings);
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

      draw_text_in_rect(font_manager, hasLabel.label.c_str(), draw_rect,
                        hasLabel.alignment, font_col, SHOW_TEXT_OVERFLOW_DEBUG,
                        stroke, shadow);
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
  }

  void render(const UIContext<InputAction> &context,
              const FontManager &font_manager, const Entity &entity) const {
    const UIComponent &cmp = entity.get<UIComponent>();
    if (cmp.should_hide || entity.has<ShouldHide>())
      return;

    if (cmp.font_name != UIComponent::UNSET_FONT) {
      const_cast<FontManager &>(font_manager).set_active(cmp.font_name);
    }

    if (entity.has<HasColor>() || entity.has<HasLabel>() ||
        entity.has<ui::HasImage>() ||
        entity.has<texture_manager::HasTexture>() ||
        entity.has<FocusClusterRoot>()) {
      render_me(context, font_manager, entity);
    }

    // NOTE: i dont think we need this TODO
    // for (EntityID child : cmp.children) {
    // render(context, font_manager, AutoLayout::to_ent_static(child));
    // }
  }

  virtual void for_each_with_derived(const Entity &entity,
                                     const UIContext<InputAction> &context,
                                     const FontManager &font_manager,
                                     float) const override {
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

    if (!context.is_modal_active()) {
      for (auto &cmd : context.render_cmds) {
        auto id = cmd.id;
        auto &ent = EntityHelper::getEntityForIDEnforce(id);
        render(context, font_manager, ent);
      }
    } else {
      std::vector<RenderInfo> non_modal_cmds;
      std::map<EntityID, std::vector<RenderInfo>> modal_cmds;

      for (auto &cmd : context.render_cmds) {
        EntityID modal_owner = context.ROOT;
        for (auto it = context.modal_stack.rbegin();
             it != context.modal_stack.rend(); ++it) {
          if (is_entity_in_tree(*it, cmd.id)) {
            modal_owner = *it;
            break;
          }
        }

        if (modal_owner == context.ROOT) {
          non_modal_cmds.push_back(cmd);
        } else {
          modal_cmds[modal_owner].push_back(cmd);
        }
      }

      for (auto &cmd : non_modal_cmds) {
        auto id = cmd.id;
        auto &ent = EntityHelper::getEntityForIDEnforce(id);
        render(context, font_manager, ent);
      }

      for (auto modal_id : context.modal_stack) {
        OptEntity modal_opt = EntityHelper::getEntityForID(modal_id);
        if (!modal_opt.has_value())
          continue;
        Entity &modal_ent = modal_opt.asE();
        if (!modal_ent.has<IsModal>())
          continue;

        const auto &modal = modal_ent.get<IsModal>();
        if (!modal.active)
          continue;

        auto res = window_manager::fetch_current_resolution();
        draw_rectangle(RectangleType{0, 0, static_cast<float>(res.width),
                                     static_cast<float>(res.height)},
                       modal.backdrop_color);

        auto it = modal_cmds.find(modal_id);
        if (it == modal_cmds.end())
          continue;
        for (auto &cmd : it->second) {
          auto id = cmd.id;
          auto &ent = EntityHelper::getEntityForIDEnforce(id);
          render(context, font_manager, ent);
        }
      }
    }
    context.render_cmds.clear();
  }
};

} // namespace ui

} // namespace afterhours
