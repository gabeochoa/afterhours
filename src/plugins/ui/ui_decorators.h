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

    auto arm = [&](int id, float x, float y, float aw, float ah) {
      div(ctx, mk(parent, id),
          ComponentConfig{}
              .with_size(ComponentSize{pixels(aw), pixels(ah)})
              .with_absolute_position()
              .with_translate(x, y)
              .with_custom_background(color)
              .with_skip_tabbing(true)
              .with_debug_name("bracket"));
    };

    // Top-left
    arm(0, -thickness, -thickness, bracket_size, thickness);
    arm(1, -thickness, -thickness, thickness, bracket_size);
    // Top-right
    arm(2, w - bracket_size + thickness, -thickness, bracket_size, thickness);
    arm(3, w, -thickness, thickness, bracket_size);
    // Bottom-left
    arm(4, -thickness, h, bracket_size, thickness);
    arm(5, -thickness, h - bracket_size + thickness, thickness, bracket_size);
    // Bottom-right
    arm(6, w - bracket_size + thickness, h, bracket_size, thickness);
    arm(7, w, h - bracket_size + thickness, thickness, bracket_size);
  };
}

} // namespace imm
} // namespace ui
} // namespace afterhours
