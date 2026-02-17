#pragma once

#include "../../input_system.h"
#include "../component_init.h"
#include "../element_result.h"
#include "../entity_management.h"
#include "../rendering.h"
#include "../rounded_corners.h"
#include "text_area_state.h"
#include "utils.h"
#include <algorithm>

namespace afterhours {
namespace text_input {

using namespace afterhours::ui;
using namespace afterhours::ui::imm;

/// Creates a multiline text input field (text area).
///
/// @param ctx The UI context
/// @param ep_pair Entity-parent pair for hierarchy
/// @param text Reference to the string that will be edited
/// @param config Component configuration
///
/// Features:
/// - Click to focus, keyboard input when focused
/// - Enter to insert newline
/// - Up/Down arrows to navigate lines
/// - Left/Right arrows to move cursor
/// - Home/End to go to line start/end
/// - Visual cursor that blinks when focused
/// - Word wrapping (optional)
/// - Vertical scrolling when content exceeds viewport
/// - Full UTF-8/CJK support
///
/// Configuration:
/// - with_line_height(Size) - Line height, e.g. pixels(18) (default: 20px)
/// - with_word_wrap(bool) - Enable word wrapping (default: true)
/// - with_max_lines(size_t) - Maximum lines, 0 = unlimited (default: 0)
///
/// Usage:
/// ```cpp
/// std::string message;
/// if (text_area(ctx, mk(parent), message,
///               ComponentConfig{}
///                   .with_size(ComponentSize{pixels(300), pixels(100)})
///                   .with_line_height(pixels(18))
///                   .with_max_lines(5))) {
///   // Text was changed
/// }
/// ```
ElementResult text_area(HasUIContext auto &ctx, EntityParent ep_pair,
                        std::string &text,
                        ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);
  using InputAction =
      typename std::remove_reference_t<decltype(ctx)>::value_type;

  // Get line height from config or default (extract pixel value from Size)
  float line_height = config.text_area_line_height.value_or(pixels(20.f)).value;

  // Initialize state
  auto &state = init_state<HasTextAreaState>(
      entity,
      [&](HasTextAreaState &s) {
        // Update area config from component config
        s.area_config.line_height = line_height;
        s.area_config.word_wrap = config.text_area_word_wrap;
        s.area_config.max_lines = config.text_area_max_lines;

        if (s.text() != text) {
          s.storage.clear();
          s.storage.insert(0, text);
          s.cursor_position = text.size();
          s.rebuild_line_index();
        }
        s.changed_since = false;
      },
      text);

  // Apply default size if not set
  if (config.size.is_default) {
    config.size = ComponentSize(pixels(200), pixels(100));
  }

  config.flex_direction = FlexDirection::Column;
  init_component(ctx, ep_pair, config, ComponentType::TextInput, false,
                 "text_area");

  auto base_corners = RoundedCorners(
      config.rounded_corners.value_or(ctx.theme.rounded_corners));

  // Create the text area container
  auto field_result =
      div(ctx, mk(entity, 0),
          ComponentConfig::inherit_from(config, "text_area_field")
              .with_size(config.size)
              .with_background(Theme::Usage::Secondary)
              .with_rounded_corners(base_corners)
              .with_alignment(TextAlignment::Left)
              .with_padding(Padding{.top = h720(4),
                                    .bottom = h720(4),
                                    .left = w1280(6),
                                    .right = w1280(6)})
              .with_render_layer(config.render_layer + 1));

  auto &field_entity = field_result.ent();
  auto &field_cmp = field_entity.template get<UIComponent>();

  // Calculate viewport dimensions
  // Use computed (resolved) values, with fallback to config size on first frame
  // Default padding estimate: ~8px vertical, ~12px horizontal at 720p baseline
  float computed_height = field_cmp.computed[Axis::Y] > 0
                              ? field_cmp.computed[Axis::Y]
                              : config.size.y_axis.value;
  float computed_width = field_cmp.computed[Axis::X] > 0
                             ? field_cmp.computed[Axis::X]
                             : config.size.x_axis.value;
  float pad_y = field_cmp.computed_padd[Axis::Y] > 0
                    ? field_cmp.computed_padd[Axis::Y]
                    : 8.f; // Fallback padding
  float pad_x = field_cmp.computed_padd[Axis::X] > 0
                    ? field_cmp.computed_padd[Axis::X]
                    : 12.f; // Fallback padding
  float viewport_height = std::max(computed_height - pad_y, line_height);
  float viewport_width = std::max(computed_width - pad_x, 50.f);

  // Build the lines to display
  std::string display_text = state.text();
  std::vector<std::string> lines;

  // Split text into lines
  size_t pos = 0;
  while (pos <= display_text.size()) {
    size_t end = display_text.find('\n', pos);
    if (end == std::string::npos)
      end = display_text.size();
    lines.push_back(display_text.substr(pos, end - pos));
    if (end >= display_text.size())
      break;
    pos = end + 1;
  }
  if (lines.empty())
    lines.push_back("");

  // Get current cursor row/column
  auto cursor_rc = state.cursor_position_rc();

  // Ensure cursor is visible
  state.ensure_cursor_visible(viewport_height);

  // Calculate first visible line
  size_t first_visible_line =
      static_cast<size_t>(state.scroll_offset_y / line_height);
  size_t visible_line_count =
      static_cast<size_t>(viewport_height / line_height) + 1;

  // Render each visible line as a separate div with fixed height
  // This prevents auto-scaling which would make text fill the entire container
  for (size_t i = first_visible_line;
       i < lines.size() && i < first_visible_line + visible_line_count; ++i) {
    size_t line_idx = i - first_visible_line;
    std::string line_text =
        lines[i].empty() ? " " : lines[i]; // Space prevents zero-height

    div(ctx, mk(field_entity, static_cast<int>(line_idx)),
        ComponentConfig{}
            .with_label(line_text)
            .with_size(
                ComponentSize{pixels(viewport_width), pixels(line_height)})
            .with_font(config.font_name == UIComponent::UNSET_FONT
                           ? UIComponent::DEFAULT_FONT
                           : config.font_name,
                       config.font_size)
            .with_custom_text_color(
                config.custom_text_color.value_or(ctx.theme.font))
            .with_alignment(TextAlignment::Left)
            .with_skip_tabbing(true)
            .with_render_layer(config.render_layer + 2)
            .with_debug_name("text_area_line"));
  }

  // Update focus state
  field_entity.template addComponentIfMissing<InFocusCluster>();
  bool field_has_focus = ctx.has_focus(field_entity.id);
  bool parent_has_focus = ctx.has_focus(entity.id);
  state.is_focused = field_has_focus || parent_has_focus;

  // Render cursor when focused
  if (state.is_focused) {
    bool show_cursor = update_blink(state, 0.016f);

    auto font_manager = EntityHelper::get_singleton_cmp<FontManager>();

    // Use computed padding for positioning
    float pad_left = field_cmp.computed_padd[Axis::left];
    float pad_top = field_cmp.computed_padd[Axis::top];

    float cursor_x = pad_left;
    float cursor_height = std::max(line_height * 0.8f, 12.f);

    if (font_manager && cursor_rc.row >= first_visible_line) {
      std::string font_name = config.font_name == UIComponent::UNSET_FONT
                                  ? UIComponent::DEFAULT_FONT
                                  : config.font_name;
      Font font = font_manager->get_font(font_name);

      // Get the current line text up to cursor
      size_t current_line_idx = cursor_rc.row;
      if (current_line_idx < lines.size()) {
        std::string text_before_cursor =
            lines[current_line_idx].substr(0, cursor_rc.column);

        // Measure text width to position cursor using configured font size
        if (!text_before_cursor.empty()) {
          // Resolve font_size to pixels
          float screen_height = 720.f;
          if (auto *pcr = EntityHelper::get_singleton_cmp<
                  window_manager::ProvidesCurrentResolution>()) {
            screen_height = static_cast<float>(pcr->current_resolution.height);
          }
          float resolved_font_size =
              resolve_to_pixels(config.font_size, screen_height);
          Vector2Type text_size = measure_text(font, text_before_cursor.c_str(),
                                               resolved_font_size, 1.f);
          cursor_x = pad_left + text_size.x;
        }
      }
    }

    // Calculate cursor Y position based on row
    float cursor_y =
        pad_top +
        static_cast<float>(cursor_rc.row - first_visible_line) * line_height +
        (line_height - cursor_height) / 2.f;

    // Only render cursor if visible in viewport
    if (cursor_rc.row >= first_visible_line &&
        cursor_rc.row < first_visible_line + visible_line_count) {
      // Use absolute positioning to avoid flex layout interference
      div(ctx, mk(field_entity, 1000),
          ComponentConfig()
              .with_size(ComponentSize(pixels(2), pixels(cursor_height)))
              .with_custom_background(
                  config.custom_text_color.value_or(ctx.theme.font))
              .with_absolute_position()
              .with_translate(cursor_x, cursor_y)
              .with_opacity(show_cursor ? 1.0f : 0.0f)
              .with_skip_tabbing(true)
              .with_debug_name("text_area_cursor")
              .with_render_layer(config.render_layer + 10));
    }
  }

  // Click to focus
  field_entity.template addComponentIfMissing<HasClickListener>(
      [&ctx](Entity &ent) {
        ctx.set_focus(ent.id);
        if (ent.has<HasTextAreaState>())
          reset_blink(ent.get<HasTextAreaState>());
      });

  // Handle input when focused
  if (state.is_focused) {
    bool text_changed = false;

    // Character input
    for (int key = input::get_char_pressed(); key > 0;
         key = input::get_char_pressed()) {
      if (insert_char(state, key)) {
        reset_blink(state);
        reset_preferred_column(state);
        state.rebuild_line_index();
        text_changed = true;
      }
    }

    // Enter key - insert newline
    if (ctx.pressed(InputAction::WidgetPress)) {
      if (insert_newline(state)) {
        reset_blink(state);
        text_changed = true;
      }
    }

    // Backspace
    if (ctx.pressed(InputAction::TextBackspace)) {
      if (delete_before_cursor_multiline(state)) {
        reset_blink(state);
        text_changed = true;
      }
    }

    // Delete
    if (ctx.pressed(InputAction::TextDelete)) {
      if (delete_at_cursor_multiline(state)) {
        reset_blink(state);
        text_changed = true;
      }
    }

    // Home - go to line start
    if (ctx.pressed(InputAction::TextHome)) {
      move_to_line_start(state);
      reset_blink(state);
    }

    // End - go to line end
    if (ctx.pressed(InputAction::TextEnd)) {
      move_to_line_end(state);
      reset_blink(state);
    }

    // Left arrow
    if (ctx.pressed(InputAction::WidgetLeft)) {
      move_cursor_left(state);
      reset_preferred_column(state);
      reset_blink(state);
    }

    // Right arrow
    if (ctx.pressed(InputAction::WidgetRight)) {
      move_cursor_right(state);
      reset_preferred_column(state);
      reset_blink(state);
    }

    // Up arrow
    if (ctx.pressed(InputAction::WidgetUp)) {
      move_cursor_up(state);
      reset_blink(state);
    }

    // Down arrow
    if (ctx.pressed(InputAction::WidgetDown)) {
      move_cursor_down(state);
      reset_blink(state);
    }

    // Ensure cursor stays visible after any input
    if (text_changed) {
      state.ensure_cursor_visible(viewport_height);
    }
  }

  text = state.text();
  entity.template addComponentIfMissing<FocusClusterRoot>();
  return ElementResult{state.changed_since, entity};
}

} // namespace text_input

// Backward compatibility - expose in ui::imm namespace
namespace ui {
namespace imm {
using afterhours::text_input::text_area;
} // namespace imm
} // namespace ui

} // namespace afterhours
