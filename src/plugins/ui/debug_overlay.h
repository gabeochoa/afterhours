#pragma once

#include "../../drawing_helpers.h"
#include "../color.h"
#include "../input.h"
#include "rendering.h"
#include "systems.h"
#include "ui_core_components.h"

namespace afterhours {
namespace ui {

// Debug overlay colors
namespace debug_colors {
constexpr Color ROW_DIRECTION = {0, 100, 255, 180};    // Blue for Row
constexpr Color COLUMN_DIRECTION = {0, 200, 100, 180}; // Green for Column
constexpr Color OVERFLOW_BORDER = {255, 50, 50, 220};  // Red for overflow
constexpr Color NOWRAP_BORDER = {255, 165, 0, 180};    // Orange for NoWrap
} // namespace debug_colors

/// Debug overlay system that shows visual indicators for layout properties.
/// Toggle with the specified input action (e.g., F4).
/// Shows:
/// - Colored borders based on FlexDirection (blue = Row, green = Column)
/// - Orange border for NoWrap components
/// - Red highlights for overflow situations
template <typename InputAction>
struct LayoutDebugOverlay : SystemWithUIContext<UIComponent> {
  InputAction toggle_action;
  bool enabled = false;
  float enableCooldown = 0.f;
  float enableCooldownReset = 0.2f;

  UIContext<InputAction> *context;

  LayoutDebugOverlay(InputAction toggle_kp) : toggle_action(toggle_kp) {
    this->include_derived_children = true;
  }

  virtual ~LayoutDebugOverlay() {}

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

  virtual void once(float) override {
    this->context =
        EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();

    // Draw legend at top-right
    float legend_x = this->context->screen_bounds.x -
                     this->context->screen_bounds.width * 0.15f;
    float legend_y = 10.f;
    float font_size = 14.f;

    draw_text("Layout Debug (F4)", legend_x, legend_y, font_size,
              this->context->theme.font);
    legend_y += font_size + 2.f;

    draw_rectangle(Rectangle{legend_x, legend_y, 12, 12},
                   debug_colors::ROW_DIRECTION);
    draw_text("Row", legend_x + 16, legend_y, font_size,
              this->context->theme.font);
    legend_y += font_size + 2.f;

    draw_rectangle(Rectangle{legend_x, legend_y, 12, 12},
                   debug_colors::COLUMN_DIRECTION);
    draw_text("Column", legend_x + 16, legend_y, font_size,
              this->context->theme.font);
    legend_y += font_size + 2.f;

    draw_rectangle(Rectangle{legend_x, legend_y, 12, 12},
                   debug_colors::NOWRAP_BORDER);
    draw_text("NoWrap", legend_x + 16, legend_y, font_size,
              this->context->theme.font);
    legend_y += font_size + 2.f;

    draw_rectangle(Rectangle{legend_x, legend_y, 12, 12},
                   debug_colors::OVERFLOW_BORDER);
    draw_text("Overflow", legend_x + 16, legend_y, font_size,
              this->context->theme.font);
  }

  void for_each_with(Entity &entity, UIComponent &cmp,
                     float) override {
    if (cmp.should_hide)
      return;

    Rectangle rect = cmp.rect();

    // Skip very small components
    if (rect.width < 2.f || rect.height < 2.f)
      return;

    // Determine border color based on flex direction
    Color border_color;
    bool is_row =
        static_cast<bool>(cmp.flex_direction & FlexDirection::Row);
    bool is_column =
        static_cast<bool>(cmp.flex_direction & FlexDirection::Column);

    if (is_row) {
      border_color = debug_colors::ROW_DIRECTION;
    } else if (is_column) {
      border_color = debug_colors::COLUMN_DIRECTION;
    } else {
      // FlexDirection::None - skip
      return;
    }

    // Override with NoWrap color if set
    if (cmp.flex_wrap == FlexWrap::NoWrap) {
      border_color = debug_colors::NOWRAP_BORDER;
    }

    // Check for overflow - if any child extends beyond this component
    bool has_overflow = false;
    for (EntityID child_id : cmp.children) {
      Entity &child_ent = UICollectionHolder::getEntityForIDEnforce(child_id);
      if (!child_ent.has<UIComponent>())
        continue;
      const UIComponent &child = child_ent.get<UIComponent>();
      if (child.should_hide)
        continue;

      Rectangle child_rect = child.rect();
      bool overflows_x = (child_rect.x + child_rect.width) > (rect.x + rect.width + 1.f);
      bool overflows_y = (child_rect.y + child_rect.height) > (rect.y + rect.height + 1.f);
      if (overflows_x || overflows_y) {
        has_overflow = true;
        break;
      }
    }

    if (has_overflow) {
      border_color = debug_colors::OVERFLOW_BORDER;
    }

    // Draw border
    draw_rectangle_outline(rect, border_color, 2.0f);

    // Draw small direction indicator in top-left corner
    float indicator_size = fmin(8.f, fmin(rect.width, rect.height) * 0.2f);
    if (indicator_size >= 4.f) {
      float ind_x = rect.x + 2.f;
      float ind_y = rect.y + 2.f;

      if (is_row) {
        // Horizontal bar for Row
        draw_rectangle(Rectangle{ind_x, ind_y + indicator_size / 3.f,
                                 indicator_size, indicator_size / 3.f},
                       border_color);
      } else {
        // Vertical bar for Column
        draw_rectangle(Rectangle{ind_x + indicator_size / 3.f, ind_y,
                                 indicator_size / 3.f, indicator_size},
                       border_color);
      }
    }
  }
};

} // namespace ui
} // namespace afterhours

