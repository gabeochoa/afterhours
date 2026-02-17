#pragma once

#include "../../drawing_helpers.h"
#include "../../ecs.h"
#include "../color.h"
#include "../input.h"
#include "components.h"
#include "systems.h"
#include "ui_core_components.h"
#include <magic_enum/magic_enum.hpp>

namespace afterhours {
namespace ui {

/// Runtime Layout Inspector - displays detailed layout properties for selected
/// components. Toggle with the specified input action (e.g., F5). Click on any
/// component in the scene to inspect it. Shows:
/// - Component name and entity ID
/// - Computed size (width x height)
/// - Position (absolute x, y)
/// - FlexDirection, JustifyContent, AlignItems
/// - Padding and Margin values
/// - Parent chain
/// - Children list with overflow status
template <typename InputAction>
struct LayoutInspector : SystemWithUIContext<UIComponent> {
  InputAction toggle_action;
  bool enabled = false;
  float enableCooldown = 0.f;
  float enableCooldownReset = 0.2f;

  UIContext<InputAction> *context;
  EntityID selected_id = -1;
  bool panel_hovered = false;

  // Panel dimensions
  static constexpr float PANEL_WIDTH = 280.f;
  static constexpr float PANEL_PADDING = 8.f;
  static constexpr float LINE_HEIGHT = 16.f;
  static constexpr float FONT_SIZE = 12.f;

  LayoutInspector(InputAction toggle_kp) : toggle_action(toggle_kp) {
    this->include_derived_children = true;
  }

  virtual ~LayoutInspector() {}

  virtual bool should_run(float dt) override {
    enableCooldown -= dt;

    if (enableCooldown < 0) {
      enableCooldown = enableCooldownReset;
      input::PossibleInputCollector inpc = input::get_input_collector();
      for (auto &actions_done : inpc.inputs()) {
        if (static_cast<InputAction>(actions_done.action) == toggle_action) {
          enabled = !enabled;
          if (!enabled) {
            selected_id = -1;
          }
          break;
        }
      }
    }
    return enabled;
  }

  std::string get_component_name(EntityID id) const {
    auto opt = EntityQuery().whereID(id).gen_first();
    if (!opt)
      return fmt::format("entity_{}", id);
    Entity &ent = opt.asE();
    if (ent.has<UIComponentDebug>()) {
      return ent.get<UIComponentDebug>().name();
    }
    return fmt::format("entity_{}", id);
  }

  std::string flex_direction_str(FlexDirection fd) const {
    // FlexDirection is a bitmask, check specific flags
    if (static_cast<bool>(fd & FlexDirection::Row))
      return "Row";
    if (static_cast<bool>(fd & FlexDirection::Column))
      return "Column";
    return "None";
  }

  template <typename E> std::string enum_str(E value) const {
    return std::string(magic_enum::enum_name(value));
  }

  void draw_panel_line(float &y, const std::string &label,
                       const std::string &value, Color value_color) const {
    float panel_x = context->screen_bounds.x - PANEL_WIDTH - 10.f;
    draw_text(label.c_str(), panel_x + PANEL_PADDING, y, FONT_SIZE,
              colors::opacity_pct(context->theme.font, 0.7f));
    draw_text(value.c_str(), panel_x + PANEL_PADDING + 100.f, y, FONT_SIZE,
              value_color);
    y += LINE_HEIGHT;
  }

  void draw_panel_header(float &y, const std::string &text) const {
    float panel_x = context->screen_bounds.x - PANEL_WIDTH - 10.f;
    y += 4.f;
    draw_text(text.c_str(), panel_x + PANEL_PADDING, y, FONT_SIZE,
              context->theme.primary);
    y += LINE_HEIGHT;
  }

  virtual void once(float) override {
    this->context =
        EntityHelper::get_singleton_cmp<ui::UIContext<InputAction>>();
    panel_hovered = false;

    // Draw panel background
    float panel_x = context->screen_bounds.x - PANEL_WIDTH - 10.f;
    float panel_y = 10.f;
    float panel_height = context->screen_bounds.height * 0.8f;

    Rectangle panel_rect = {panel_x, panel_y, PANEL_WIDTH, panel_height};
    panel_hovered = is_mouse_inside(context->mouse.pos, panel_rect);

    // Panel background
    draw_rectangle(panel_rect,
                   colors::opacity_pct(context->theme.surface, 0.95f));
    draw_rectangle_outline(panel_rect, context->theme.primary, 2.f);

    // Title
    float y = panel_y + PANEL_PADDING;
    draw_text("Layout Inspector (F5)", panel_x + PANEL_PADDING, y, 14.f,
              context->theme.font);
    y += 20.f;

    // Divider
    draw_rectangle(Rectangle{panel_x + PANEL_PADDING, y,
                             PANEL_WIDTH - PANEL_PADDING * 2, 1.f},
                   colors::opacity_pct(context->theme.font, 0.3f));
    y += 8.f;

    if (selected_id < 0) {
      draw_text("Click a component to inspect", panel_x + PANEL_PADDING, y,
                FONT_SIZE, colors::opacity_pct(context->theme.font, 0.5f));
      return;
    }

    // Get selected component
    auto opt = EntityQuery().whereID(selected_id).gen_first();
    if (!opt) {
      draw_text("Component not found", panel_x + PANEL_PADDING, y, FONT_SIZE,
                context->theme.from_usage(Theme::Usage::Error));
      selected_id = -1;
      return;
    }

    Entity &ent = opt.asE();
    if (!ent.has<UIComponent>()) {
      draw_text("No UIComponent", panel_x + PANEL_PADDING, y, FONT_SIZE,
                context->theme.from_usage(Theme::Usage::Error));
      selected_id = -1;
      return;
    }

    const UIComponent &cmp = ent.get<UIComponent>();

    // Component info
    draw_panel_header(y, "COMPONENT");
    draw_panel_line(y, "Name:", get_component_name(selected_id),
                    context->theme.font);
    draw_panel_line(y, "Entity ID:", std::to_string(selected_id),
                    context->theme.font);

    // Size
    draw_panel_header(y, "SIZE");
    draw_panel_line(y, "Width:", fmt::format("{:.1f}px", cmp.width()),
                    context->theme.font);
    draw_panel_line(y, "Height:", fmt::format("{:.1f}px", cmp.height()),
                    context->theme.font);

    // Position
    draw_panel_header(y, "POSITION");
    draw_panel_line(y, "X:", fmt::format("{:.1f}", cmp.x()),
                    context->theme.font);
    draw_panel_line(y, "Y:", fmt::format("{:.1f}", cmp.y()),
                    context->theme.font);

    // Flex properties
    draw_panel_header(y, "FLEX LAYOUT");
    draw_panel_line(y, "Direction:", flex_direction_str(cmp.flex_direction),
                    context->theme.primary);
    draw_panel_line(y, "Justify:", enum_str(cmp.justify_content),
                    context->theme.font);
    draw_panel_line(y, "Align:", enum_str(cmp.align_items),
                    context->theme.font);
    draw_panel_line(y, "SelfAlign:", enum_str(cmp.self_align),
                    cmp.self_align != SelfAlign::Auto ? context->theme.primary
                                                      : context->theme.font);
    draw_panel_line(y, "FlexWrap:", enum_str(cmp.flex_wrap),
                    cmp.flex_wrap == FlexWrap::NoWrap ? Color{255, 165, 0, 255}
                                                      // Orange for NoWrap
                                                      : context->theme.font);

    // Spacing
    draw_panel_header(y, "SPACING");
    draw_panel_line(y, "Padding:",
                    fmt::format("{:.0f} {:.0f} {:.0f} {:.0f}",
                                cmp.computed_padd[Axis::top],
                                cmp.computed_padd[Axis::right],
                                cmp.computed_padd[Axis::bottom],
                                cmp.computed_padd[Axis::left]),
                    context->theme.font);
    draw_panel_line(y, "Margin:",
                    fmt::format("{:.0f} {:.0f} {:.0f} {:.0f}",
                                cmp.computed_margin[Axis::top],
                                cmp.computed_margin[Axis::right],
                                cmp.computed_margin[Axis::bottom],
                                cmp.computed_margin[Axis::left]),
                    context->theme.font);

    // Parent chain
    draw_panel_header(y, "HIERARCHY");
    if (cmp.parent >= 0) {
      draw_panel_line(y, "Parent:", get_component_name(cmp.parent),
                      context->theme.font);
    } else {
      draw_panel_line(y, "Parent:", "(root)", context->theme.font);
    }
    draw_panel_line(y, "Children:", std::to_string(cmp.children.size()),
                    context->theme.font);

    // Check for overflow
    bool has_overflow = false;
    Rectangle rect = cmp.rect();
    for (EntityID child_id : cmp.children) {
      auto child_opt = EntityQuery().whereID(child_id).gen_first();
      if (!child_opt)
        continue;
      Entity &child_ent = child_opt.asE();
      if (!child_ent.has<UIComponent>())
        continue;
      const UIComponent &child = child_ent.get<UIComponent>();
      if (child.should_hide)
        continue;

      Rectangle child_rect = child.rect();
      if ((child_rect.x + child_rect.width) > (rect.x + rect.width + 1.f) ||
          (child_rect.y + child_rect.height) > (rect.y + rect.height + 1.f)) {
        has_overflow = true;
        break;
      }
    }

    if (has_overflow) {
      y += 8.f;
      draw_text("! OVERFLOW DETECTED", panel_x + PANEL_PADDING, y, FONT_SIZE,
                context->theme.from_usage(Theme::Usage::Error));
    }

    // Draw selection highlight on the component
    draw_rectangle_outline(cmp.rect(), context->theme.primary, 3.f);
  }

  void for_each_with(Entity &entity, UIComponent &cmp, float) override {
    if (cmp.should_hide)
      return;
    if (panel_hovered)
      return; // Don't select through panel

    Rectangle rect = cmp.rect();
    if (rect.width < 2.f || rect.height < 2.f)
      return;

    // Check for click on this component
    bool is_hovered = is_mouse_inside(context->mouse.pos, rect);
    bool left_clicked = input::is_mouse_button_released(0);

    if (is_hovered && left_clicked) {
      selected_id = entity.id;
    }

    // Draw hover indicator
    if (is_hovered && entity.id != selected_id) {
      draw_rectangle_outline(
          rect, colors::opacity_pct(context->theme.primary, 0.5f), 1.f);
    }
  }
};

} // namespace ui
} // namespace afterhours
