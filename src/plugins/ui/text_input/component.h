#pragma once

#include "state.h"
#include "utils.h"
#include "../component_init.h"
#include "../element_result.h"
#include "../entity_management.h"
#include "../rendering.h"
#include "../rounded_corners.h"
#include "../../input_system.h"

namespace afterhours {
namespace text_input {

using namespace afterhours::ui;
using namespace afterhours::ui::imm;

/// Creates a single-line text input field.
///
/// @param ctx The UI context
/// @param ep_pair Entity-parent pair for hierarchy
/// @param text Reference to the string that will be edited
/// @param config Component configuration
///
/// Features:
/// - Click to focus, keyboard input when focused
/// - Backspace to delete, Enter to submit
/// - Left/Right arrows to move cursor
/// - Home/End to jump to start/end
/// - Visual cursor that blinks when focused
/// - Full UTF-8/CJK support
///
/// Usage:
/// ```cpp
/// std::string username;
/// if (text_input(ctx, mk(parent), username,
///                ComponentConfig{}.with_label("Username"))) {
///   // Text was changed
/// }
/// ```
ElementResult text_input(HasUIContext auto &ctx, EntityParent ep_pair,
                         std::string &text,
                         ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);
  using InputAction =
      typename std::remove_reference_t<decltype(ctx)>::value_type;

  // Initialize state
  auto &state = _init_state<HasTextInputState>(
      entity,
      [&](HasTextInputState &s) {
        if (s.text() != text) {
          s.storage.clear();
          s.storage.insert(0, text);
          s.cursor_position = text.size();
        }
        s.changed_since = false;
      },
      text);

  // Extract label before clearing config
  std::string label = config.label;
  config.label = "";
  bool has_label = !label.empty();

  // Apply default size
  if (config.size.is_default) {
    auto def =
        UIStylingDefaults::get().get_component_config(ComponentType::TextInput);
    config.size = def ? def->size
                      : ComponentSize(pixels(default_component_size.x * 1.5f),
                                      pixels(default_component_size.y));
  }

  config.flex_direction = FlexDirection::Row;
  _init_component(ctx, ep_pair, config, ComponentType::TextInput, false,
                  "text_input");

  auto base_corners = RoundedCorners(
      config.rounded_corners.value_or(ctx.theme.rounded_corners));
  auto field_size = has_label ? config.size._scale_x(0.5f) : config.size;

  // Create label
  if (has_label) {
    div(ctx, mk(entity, 0),
        ComponentConfig::inherit_from(config, "text_input_label")
            .with_size(field_size)
            .with_label(label)
            .with_background(Theme::Usage::Primary)
            .with_rounded_corners(base_corners.right_sharp())
            .with_skip_tabbing(true)
            .with_render_layer(config.render_layer))
        .ent()
        .template addComponentIfMissing<InFocusCluster>();
  }

  // Build display text (apply mask if configured)
  std::string display_text = state.text();
  size_t display_cursor_pos = state.cursor_position;

  if (config.mask_char) {
    // Count UTF-8 codepoints for mask and cursor position
    size_t codepoint_count = 0;
    size_t codepoints_before_cursor = 0;
    for (size_t i = 0; i < display_text.size();) {
      if (i < state.cursor_position)
        codepoints_before_cursor++;
      i += utf8_char_length(display_text, i);
      codepoint_count++;
    }
    display_text = std::string(codepoint_count, *config.mask_char);
    display_cursor_pos = codepoints_before_cursor;
  }

  // Create input field container
  auto field_result =
      div(ctx, mk(entity, has_label ? 1 : 0),
          ComponentConfig::inherit_from(config, "text_input_field")
              .with_size(field_size)
              .with_background(Theme::Usage::Secondary)
              .with_rounded_corners(has_label ? base_corners.left_sharp()
                                              : base_corners)
              .with_alignment(TextAlignment::Left)
              .with_padding(Padding{.top = pixels(5),
                                    .bottom = pixels(5),
                                    .left = pixels(10),
                                    .right = pixels(10)})
              .with_render_layer(config.render_layer + 1));

  auto &field_entity = field_result.ent();

  // Ensure HasLabel exists and set display text
  field_entity.template addComponentIfMissing<HasLabel>().label = display_text;

  // Update focus state - check if this field OR the parent container has focus
  field_entity.template addComponentIfMissing<InFocusCluster>();
  bool field_has_focus = ctx.has_focus(field_entity.id);
  bool parent_has_focus = ctx.has_focus(entity.id);
  state.is_focused = field_has_focus || parent_has_focus;

  // Render cursor as overlay when focused
  if (state.is_focused) {
    bool show_cursor = update_blink(state, 0.016f);

    // Calculate cursor position using the SAME font size as text rendering
    // Text rendering uses position_text_ex which may auto-size the font to fit
    auto &field_cmp = field_entity.template get<UIComponent>();
    auto font_manager = EntityHelper::get_singleton_cmp<FontManager>();

    float cursor_x = 5.f; // Default: text margin from position_text_ex
    float cursor_height = std::max(config.font_size * 0.9f, 16.f);
    float actual_font_size = config.font_size;

    if (font_manager) {
      // Get the actual rendered font size by calling position_text_ex on the
      // full text This accounts for auto-sizing when text doesn't fit
      constexpr Vector2Type TEXT_MARGIN{5.f, 5.f};
      TextPositionResult full_text_result = position_text_ex(
          *font_manager, display_text.empty() ? " " : display_text,
          field_cmp.rect(), TextAlignment::Left, TEXT_MARGIN);
      actual_font_size = full_text_result.rect.height;
      cursor_height = std::max(actual_font_size * 0.9f, 16.f);

      // Now measure the text BEFORE cursor using the actual rendered font size
      std::string font_name = config.font_name == UIComponent::UNSET_FONT
                                  ? UIComponent::DEFAULT_FONT
                                  : config.font_name;
      Font font = font_manager->get_font(font_name);

      if (!display_text.empty() && display_cursor_pos > 0) {
        size_t safe_pos = std::min(display_cursor_pos, display_text.size());
        std::string text_before = display_text.substr(0, safe_pos);
        Vector2Type text_size =
            measure_text(font, text_before.c_str(), actual_font_size, 1.f);
        cursor_x = TEXT_MARGIN.x + text_size.x;
      } else {
        cursor_x = TEXT_MARGIN.x;
      }
    }

    // Center cursor vertically in field
    float field_height = field_cmp.computed[Axis::Y];
    float cursor_y = (field_height - cursor_height) / 2.f;

    // Cursor is a thin vertical bar
    // Note: Width must be >= 8px to survive 8pt grid snapping at high DPI
    // (grid unit scales with screen, e.g. ~11px at 1080p, so 2-4px rounds to 0)
    auto cursor_result =
        div(ctx, mk(field_entity, 0),
            ComponentConfig()
                .with_size(ComponentSize(pixels(8), pixels(cursor_height)))
                .with_custom_background(ctx.theme.font) // Use theme text color
                .with_translate(cursor_x,
                                cursor_y) // Position cursor aligned with text
                .with_opacity(show_cursor ? 1.0f : 0.0f) // Blink
                .with_skip_tabbing(true)
                .with_debug_name("cursor")
                .with_render_layer(config.render_layer + 10));
  }

  // Click to focus
  field_entity.template addComponentIfMissing<HasClickListener>(
      [&ctx](Entity &ent) {
        ctx.set_focus(ent.id);
        if (ent.has<HasTextInputState>())
          reset_blink(ent.get<HasTextInputState>());
      });

  // TODO: Implement horizontal scrolling when text exceeds field width

  // Handle input when focused
  if (state.is_focused) {
    // Character input
    for (int key = input::get_char_pressed(); key > 0;
         key = input::get_char_pressed()) {
      if (insert_char(state, key))
        reset_blink(state);
    }

    // Key actions
    if (ctx.pressed(InputAction::TextBackspace) && delete_before_cursor(state))
      reset_blink(state);
    if (ctx.pressed(InputAction::TextDelete) && delete_at_cursor(state))
      reset_blink(state);
    if (ctx.pressed(InputAction::TextHome)) {
      state.cursor_position = 0;
      reset_blink(state);
    }
    if (ctx.pressed(InputAction::TextEnd)) {
      state.cursor_position = state.text_size();
      reset_blink(state);
    }
    if (ctx.pressed(InputAction::WidgetLeft)) {
      move_cursor_left(state);
      reset_blink(state);
    }
    if (ctx.pressed(InputAction::WidgetRight)) {
      move_cursor_right(state);
      reset_blink(state);
    }
    if (ctx.pressed(InputAction::WidgetPress) &&
        entity.has<HasTextInputListener>())
      if (auto &listener = entity.get<HasTextInputListener>();
          listener.on_submit)
        listener.on_submit(entity);
  }

  text = state.text();
  entity.template addComponentIfMissing<FocusClusterRoot>();
  return ElementResult{state.changed_since, entity};
}

} // namespace text_input

// Backward compatibility - expose in ui::imm namespace
namespace ui {
namespace imm {
using afterhours::text_input::text_input;
} // namespace imm
} // namespace ui

} // namespace afterhours

