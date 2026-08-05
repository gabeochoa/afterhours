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

// The field's own top+bottom padding, below. Auto-grow has to add it back or
// the last line sits under the bottom edge.
inline constexpr float kVerticalPadding = 8.f;

/// Creates a multiline text input field (text area).
///
/// @param ctx The UI context
/// @param ep_pair Entity-parent pair for hierarchy
/// @param text Reference to the string that will be edited
/// @param config Component configuration
///
/// Features:
/// - Click to focus, keyboard input when focused
/// - Enter to insert newline (or submit, see with_submit_on_enter)
/// - Up/Down arrows move by VISUAL row, Left/Right by character
/// - Alt/Ctrl+Left/Right move by word; Alt/Ctrl+Backspace/Delete erase a word
/// - Home/End go to the ends of the visual row, not the source line
/// - Mouse wheel scrolls when the content overflows
/// - Visual cursor that blinks when focused
/// - Word wrapping (optional)
/// - Full UTF-8/CJK support
///
/// Not supported yet: selection, clipboard, undo, and click-to-position.
/// Clicking focuses the field but does not move the caret.
///
/// Configuration:
/// - with_line_height(Size) - Line height, e.g. pixels(18) (default: 20px)
/// - with_word_wrap(bool) - Enable word wrapping (default: true)
/// - with_max_lines(size_t) - Maximum lines, 0 = unlimited (default: 0)
/// - with_auto_grow(bool) - Height follows content, capped by max_lines
/// - with_submit_on_enter(bool) - Enter submits, Shift+Enter breaks the line
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
///
/// A chat composer that grows to five lines and sends on Enter:
/// ```cpp
/// text_area(ctx, mk(parent), draft,
///           ComponentConfig{}
///               .with_size(ComponentSize{percent(1.f), pixels(36)})
///               .with_auto_grow()
///               .with_max_lines(5)
///               .with_submit_on_enter());
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

  // Auto-grow uses the PREVIOUS frame's wrap, which is the only one that
  // exists while the tree is being built. It converges in a frame, the same
  // deal text_input's height-derived font size makes.
  if (config.text_area_auto_grow) {
    const size_t rows = std::max<size_t>(1, state.layout_cache.line_count());
    const size_t capped = config.text_area_max_lines > 0
                              ? std::min(rows, config.text_area_max_lines)
                              : rows;
    config.size.y_axis =
        pixels(static_cast<float>(capped) * line_height + kVerticalPadding);
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
              // Lines past the bottom of a fixed-height field must not paint
              // over whatever sits below it.
              .with_overflow(Overflow::Hidden)
              .with_render_layer(config.render_layer + 1));

  auto &field_entity = field_result.ent();
  auto &field_cmp = field_entity.template get<UIComponent>();

  // Calculate viewport dimensions
  // Use computed (resolved) values, with fallback to config size on first frame
  // Default padding estimate: ~8px vertical, ~12px horizontal at 720p baseline
  // Auto-grow already fixed the height for THIS frame, so use it rather than
  // last frame's computed value: that one is a row behind, and a viewport a
  // row too short scrolls a field that fits its content perfectly.
  float computed_height = config.text_area_auto_grow
                              ? config.size.y_axis.value
                          : field_cmp.computed[Axis::Y] > 0
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

  std::string display_text = state.text();

  // Resolve the font up front: the wrap has to measure with the same face and
  // size the lines are drawn at, or text breaks somewhere other than where it
  // is painted.
  const std::string font_name = config.font_name == UIComponent::UNSET_FONT
                                    ? UIComponent::DEFAULT_FONT
                                    : config.font_name;
  float screen_height = 720.f;
  if (auto *pcr = EntityHelper::get_singleton_cmp<
          window_manager::ProvidesCurrentResolution>())
    screen_height = static_cast<float>(pcr->current_resolution.height);
  const float resolved_font_size =
      resolve_to_pixels(config.font_size, screen_height);

  // Wrap through the same primitive both renderers use, so an edited line
  // breaks exactly where a drawn one does. Without a font manager (headless)
  // fall back to the harness's approximation rather than laying out nothing.
  auto *font_manager = EntityHelper::get_singleton_cmp<FontManager>();
  auto line_width = [&](std::string_view s) -> float {
    if (!font_manager)
      return static_cast<float>(s.size()) * resolved_font_size * 0.5f;
    return measure_text(font_manager->get_font(font_name),
                        std::string(s).c_str(), resolved_font_size, 1.f)
        .x;
  };
  // The 6px horizontal padding each side is already out of viewport_width.
  const float wrap_width =
      config.text_area_word_wrap ? viewport_width : 0.f;
  state.layout_cache.rebuild(display_text, wrap_width, line_height,
                             line_width);
  const auto &vlines = state.layout_cache.lines();

  // Rows are VISUAL from here down: with wrapping on, one source line can
  // occupy several, and a caret that counted source lines would drift a row
  // further off with every wrap above it.
  const size_t cursor_row =
      state.layout_cache.line_at_offset(state.cursor_position);
  const size_t cursor_col =
      state.layout_cache.column_at_offset(state.cursor_position);

  state.ensure_cursor_visible_at_row(cursor_row, viewport_height,
                                     vlines.size());

  // Calculate first visible line
  size_t first_visible_line =
      static_cast<size_t>(state.scroll_offset_y / line_height);
  size_t visible_line_count =
      static_cast<size_t>(viewport_height / line_height) + 1;

  // Render each visible line as a separate div with fixed height
  // This prevents auto-scaling which would make text fill the entire container
  for (size_t i = first_visible_line;
       i < vlines.size() && i < first_visible_line + visible_line_count; ++i) {
    size_t line_idx = i - first_visible_line;
    std::string row = state.layout_cache.line_text(display_text, i);
    std::string line_text =
        row.empty() ? " " : row; // Space prevents zero-height

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

    // Use computed padding for positioning
    float pad_left = field_cmp.computed_padd[Axis::left];
    float pad_top = field_cmp.computed_padd[Axis::top];

    float cursor_x = pad_left;
    float cursor_height = std::max(line_height * 0.8f, 12.f);

    // Measured through the same line_width the wrap used, so the caret lands
    // on the break the text actually took.
    if (cursor_row >= first_visible_line) {
      const std::string row_text =
          state.layout_cache.line_text(display_text, cursor_row);
      cursor_x += line_width(std::string_view(row_text).substr(
          0, std::min(cursor_col, row_text.size())));
    }

    // Calculate cursor Y position based on row
    float cursor_y =
        pad_top +
        static_cast<float>(cursor_row - first_visible_line) * line_height +
        (line_height - cursor_height) / 2.f;

    constexpr float CURSOR_WIDTH = 2.0f;
    float field_width = field_cmp.computed[Axis::X];
    float pad_right = field_cmp.computed_padd[Axis::right];
    cursor_x = std::min(cursor_x, field_width - pad_right - CURSOR_WIDTH);

    // Only render cursor if visible in viewport
    if (cursor_row >= first_visible_line &&
        cursor_row < first_visible_line + visible_line_count) {
      div(ctx, mk(field_entity, 1000),
          ComponentConfig()
              .with_size(ComponentSize(pixels(CURSOR_WIDTH), pixels(cursor_height)))
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

  // Wheel scrolling. Works on hover rather than focus, which is what a wheel
  // over a box is expected to do, and is deliberately NOT consumed when the
  // content already fits: the wheel is consume-once, so a field that cannot
  // scroll must leave it for whatever scroll view encloses it.
  {
    const float max_scroll =
        std::max(0.f, static_cast<float>(vlines.size()) * line_height -
                          viewport_height);
    const RectangleType fr = field_cmp.rect();
    const bool hovered = ctx.mouse.pos.x >= fr.x &&
                         ctx.mouse.pos.x <= fr.x + fr.width &&
                         ctx.mouse.pos.y >= fr.y &&
                         ctx.mouse.pos.y <= fr.y + fr.height;
    if (max_scroll > 0.f && hovered) {
      const float wheel = input::get_mouse_wheel_move_v().y;
      if (wheel != 0.f) {
        state.scroll_offset_y = std::clamp(
            state.scroll_offset_y - wheel * line_height, 0.f, max_scroll);
      }
    }
  }

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

    // Enter. A chat composer wants Enter to send and Shift+Enter to break the
    // line; a plain text area wants Enter to break. Off by default, so the
    // long-standing behaviour is what you get unless you ask otherwise.
    if (ctx.pressed(InputAction::WidgetPress)) {
      const bool shift = input::is_key_down(keys::LEFT_SHIFT) ||
                         input::is_key_down(keys::RIGHT_SHIFT);
      const bool wants_newline =
          !config.text_area_submit_on_enter || shift;
      if (wants_newline) {
        if (insert_newline(state)) {
          reset_blink(state);
          text_changed = true;
        }
      } else if (entity.template has<HasTextInputListener>()) {
        if (auto &listener = entity.template get<HasTextInputListener>();
            listener.on_submit)
          listener.on_submit(entity);
      }
    }

    // Word-level delete (Alt/Ctrl+Backspace, Alt/Ctrl+Delete). Guarded on the
    // action existing so an app with a smaller enum still compiles, the same
    // way text_input does it.
    if constexpr (magic_enum::enum_contains<InputAction>(
                      "TextDeleteWordBack")) {
      if (ctx.pressed_or_repeat(InputAction::TextDeleteWordBack)) {
        if (delete_word_before_cursor(state)) {
          state.rebuild_line_index();
          reset_preferred_column(state);
          reset_blink(state);
          text_changed = true;
        }
      }
    }
    if constexpr (magic_enum::enum_contains<InputAction>(
                      "TextDeleteWordForward")) {
      if (ctx.pressed_or_repeat(InputAction::TextDeleteWordForward)) {
        if (delete_word_after_cursor(state)) {
          state.rebuild_line_index();
          reset_preferred_column(state);
          reset_blink(state);
          text_changed = true;
        }
      }
    }

    // Backspace
    if (ctx.pressed_or_repeat(InputAction::TextBackspace)) {
      if (delete_before_cursor_multiline(state)) {
        reset_preferred_column(state);
        reset_blink(state);
        text_changed = true;
      }
    }

    // Delete
    if (ctx.pressed_or_repeat(InputAction::TextDelete)) {
      if (delete_at_cursor_multiline(state)) {
        reset_preferred_column(state);
        reset_blink(state);
        text_changed = true;
      }
    }

    // Home/End act on the VISUAL row, not the source line: on a wrapped
    // paragraph the source version jumps to the far end of the paragraph
    // rather than the end of the row you can see.
    if (ctx.pressed_or_repeat(InputAction::TextHome)) {
      move_to_visual_line_start(state);
      reset_blink(state);
    }
    if (ctx.pressed_or_repeat(InputAction::TextEnd)) {
      move_to_visual_line_end(state);
      reset_blink(state);
    }

    // Word-level movement (Alt/Ctrl+Arrow).
    if constexpr (magic_enum::enum_contains<InputAction>("TextWordLeft")) {
      if (ctx.pressed_or_repeat(InputAction::TextWordLeft)) {
        move_cursor_word_left(state);
        reset_preferred_column(state);
        reset_blink(state);
      }
    }
    if constexpr (magic_enum::enum_contains<InputAction>("TextWordRight")) {
      if (ctx.pressed_or_repeat(InputAction::TextWordRight)) {
        move_cursor_word_right(state);
        reset_preferred_column(state);
        reset_blink(state);
      }
    }

    // Left arrow
    if (ctx.pressed_or_repeat(InputAction::WidgetLeft)) {
      move_cursor_left(state);
      reset_preferred_column(state);
      reset_blink(state);
    }

    // Right arrow
    if (ctx.pressed_or_repeat(InputAction::WidgetRight)) {
      move_cursor_right(state);
      reset_preferred_column(state);
      reset_blink(state);
    }

    // Up/Down move by VISUAL line: on a wrapped paragraph, moving by source
    // line would jump the whole paragraph at once.
    if (ctx.pressed_or_repeat(InputAction::WidgetUp)) {
      move_cursor_visual_row(state, -1, line_width);
      reset_blink(state);
    }
    if (ctx.pressed_or_repeat(InputAction::WidgetDown)) {
      move_cursor_visual_row(state, +1, line_width);
      reset_blink(state);
    }

    // Ensure cursor stays visible after any input
    if (text_changed) {
      state.layout_cache.rebuild(state.text(), wrap_width, line_height,
                                 line_width);
      state.ensure_cursor_visible_at_row(
          state.layout_cache.line_at_offset(state.cursor_position),
          viewport_height, state.layout_cache.line_count());
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
