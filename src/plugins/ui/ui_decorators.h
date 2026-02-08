#pragma once

// ============================================================
// UI Decorators — Higher-Order Components
// ============================================================
//
// Decorators are visual additions applied to an existing element
// via ElementResult::decorate(). Each decorator is a factory
// function (with_*) that returns a lambda with signature
// void(Entity&).
//
// Usage:
//   #include <afterhours/src/plugins/ui/ui_decorators.h>
//
//   button(ctx, mk(parent, 1), config)
//       .decorate(with_brackets(ctx, teal))
//       .decorate(with_grid_bg(ctx, 32.0f, gray));
//
// Creating your own decorator (in any plugin/file):
//
//   auto with_my_decoration(HasUIContext auto& ctx, /* params */) {
//       return [&ctx, /* captures */](Entity& parent) {
//           // Create child divs on parent using div(ctx, mk(parent, N), ...)
//           // mk() uses source location for unique IDs — no base_id needed.
//       };
//   }
//

#include "imm_components.h"

namespace afterhours {
namespace ui {
namespace imm {

// ============================================================
// Corner Bracket Decorations
// ============================================================

/// Factory: adds L-shaped corner brackets at each corner of an element.
///
/// ```cpp
/// div(ctx, mk(parent, 1), panel_config)
///     .decorate(with_brackets(ctx, teal, 20.0f, 2.0f));
/// ```
inline auto with_brackets(HasUIContext auto &ctx, Color color,
                          float bracket_size = 15.0f,
                          float thickness = 2.0f) {
  return [&ctx, color, bracket_size, thickness](Entity &parent) {
    UIComponent &cmp = parent.template get<UIComponent>();
    float w = cmp.computed[Axis::X];
    float h = cmp.computed[Axis::Y];

    // Skip until parent has a computed size
    if (w <= 0 || h <= 0)
      return;

    // Account for padding so brackets sit at the visual outer edge
    float pl = cmp.computed_padd[Axis::left];
    float pr = cmp.computed_padd[Axis::right];
    float pt = cmp.computed_padd[Axis::top];
    float pb = cmp.computed_padd[Axis::bottom];

    // Account for border so brackets sit outside it
    float bw = 0.0f;
    if (parent.template has<HasBorder>()) {
      bw = parent.template get<HasBorder>().border.thickness.value;
    }

    // Full visual box offset from content origin
    float left = -(pl + bw);
    float top = -(pt + bw);
    float outer_w = w + pl + pr + 2.0f * bw;
    float outer_h = h + pt + pb + 2.0f * bw;

    auto arm = [&](int id, float x, float y, float aw, float ah) {
      div(ctx, mk(parent, id),
          ComponentConfig{}
              .with_size(ComponentSize{pixels(aw), pixels(ah)})
              .with_absolute_position()
              .with_translate(x, y)
              .with_custom_background(color)
              .with_rounded_corners(RoundedCorners().all_sharp())
              .with_skip_tabbing(true)
              .with_debug_name("bracket"));
    };

    // Top-left
    arm(0, left - thickness, top - thickness, bracket_size, thickness);
    arm(1, left - thickness, top - thickness, thickness, bracket_size);
    // Top-right
    arm(2, left + outer_w - bracket_size + thickness, top - thickness,
        bracket_size, thickness);
    arm(3, left + outer_w, top - thickness, thickness, bracket_size);
    // Bottom-left
    arm(4, left - thickness, top + outer_h, bracket_size, thickness);
    arm(5, left - thickness, top + outer_h - bracket_size + thickness,
        thickness, bracket_size);
    // Bottom-right
    arm(6, left + outer_w - bracket_size + thickness, top + outer_h,
        bracket_size, thickness);
    arm(7, left + outer_w, top + outer_h - bracket_size + thickness,
        thickness, bracket_size);
  };
}

// ============================================================
// Grid / Decorative Background Pattern
// ============================================================

/// Factory: fills an element with a grid of horizontal and vertical lines.
///
/// ```cpp
/// div(ctx, mk(parent, 1), bg_config)
///     .decorate(with_grid_bg(ctx, 32.0f, colors::gray(40)));
/// ```
inline auto with_grid_bg(HasUIContext auto &ctx, float cell_size,
                         Color line_color, float line_thickness = 1.0f) {
  return [&ctx, cell_size, line_color, line_thickness](Entity &parent) {
    UIComponent &cmp = parent.template get<UIComponent>();
    float w = cmp.computed[Axis::X];
    float h = cmp.computed[Axis::Y];

    if (w <= 0 || h <= 0 || cell_size <= 0)
      return;

    // Account for padding so grid fills the full visual area
    float pl = cmp.computed_padd[Axis::left];
    float pr = cmp.computed_padd[Axis::right];
    float pt = cmp.computed_padd[Axis::top];
    float pb = cmp.computed_padd[Axis::bottom];
    float full_w = w + pl + pr;
    float full_h = h + pt + pb;

    int id = 0;

    // Vertical lines
    for (float x = cell_size; x < full_w; x += cell_size) {
      div(ctx, mk(parent, id++),
          ComponentConfig{}
              .with_size(ComponentSize{pixels(line_thickness), pixels(full_h)})
              .with_absolute_position()
              .with_translate(x - pl, -pt)
              .with_custom_background(line_color)
              .with_rounded_corners(RoundedCorners().all_sharp())
              .with_skip_tabbing(true)
              .with_debug_name("grid_v"));
    }

    // Horizontal lines
    for (float y = cell_size; y < full_h; y += cell_size) {
      div(ctx, mk(parent, id++),
          ComponentConfig{}
              .with_size(ComponentSize{pixels(full_w), pixels(line_thickness)})
              .with_absolute_position()
              .with_translate(-pl, y - pt)
              .with_custom_background(line_color)
              .with_rounded_corners(RoundedCorners().all_sharp())
              .with_skip_tabbing(true)
              .with_debug_name("grid_h"));
    }
  };
}

// ============================================================
// Quote / Blockquote
// ============================================================

/// Configuration for the quote decorator.
struct QuoteStyle {
  std::string attribution;          // e.g. "— Elder Sage"
  bool show_quote_marks = false;
  Color accent_color{200, 160, 100, 255};
  float accent_width = 4.0f;
};

/// Factory: wraps an element's label text in a blockquote style with a
/// left accent bar and optional attribution.
///
/// Note: This decorator reads the label from the parent's config and
/// re-renders it inside a quote layout. Best used on a div with a label.
///
/// ```cpp
/// div(ctx, mk(parent, 1),
///     ComponentConfig{}.with_label("The cake is a lie."))
///     .decorate(with_quote(ctx,
///         QuoteStyle{.attribution = "— GLaDOS",
///                    .accent_color = colors::info()}));
/// ```
inline auto with_quote(HasUIContext auto &ctx,
                       QuoteStyle style = QuoteStyle()) {
  return [&ctx, style](Entity &parent) {
    // Accent bar
    div(ctx, mk(parent, 0),
        ComponentConfig{}
            .with_size(
                ComponentSize{pixels(style.accent_width), percent(1.0f)})
            .with_custom_background(style.accent_color)
            .with_rounded_corners(RoundedCorners().all_sharp())
            .with_skip_tabbing(true)
            .with_debug_name("quote_accent"));

    // Attribution (if provided)
    if (!style.attribution.empty()) {
      div(ctx, mk(parent, 1),
          ComponentConfig{}
              .with_size(ComponentSize{children(), children()})
              .with_label(style.attribution)
              .with_alignment(TextAlignment::Left)
              .with_padding(Spacing::sm)
              .with_custom_text_color(
                  colors::opacity_pct(style.accent_color, 0.8f))
              .with_skip_tabbing(true)
              .with_debug_name("quote_attribution"));
    }
  };
}

} // namespace imm
} // namespace ui
} // namespace afterhours
