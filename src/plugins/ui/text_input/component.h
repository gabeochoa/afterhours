#pragma once

#include "../../clipboard.h"
#include "../../input_system.h"
#include "../../../core/key_codes.h"
#include "../../../graphics.h"
#include "../component_init.h"
#include "../element_result.h"
#include "../entity_management.h"
#include "../rendering.h"
#include "../rounded_corners.h"
#include "state.h"
#include "utils.h"

namespace afterhours {
namespace text_input {

using namespace afterhours::ui;
using namespace afterhours::ui::imm;

// Given a local x coordinate within a text field, find the byte offset
// in the display text that's closest to that position.
inline size_t pixel_x_to_byte_offset(const std::string &text, float local_x,
                                     Font font, float font_size) {
  size_t result = 0;
  float best_dist = std::abs(local_x);
  for (size_t i = 0; i < text.size();) {
    size_t clen = utf8_char_length(text, i);
    size_t next = i + clen;
    std::string sub = text.substr(0, next);
    Vector2Type sz = measure_text(font, sub.c_str(), font_size, 1.f);
    float dist = std::abs(local_x - sz.x);
    if (dist < best_dist) {
      best_dist = dist;
      result = next;
    }
    i = next;
  }
  return result;
}

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
  auto &state = init_state<HasTextInputState>(
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

  // Apply readonly/disabled from config each frame
  state.readonly = config.text_readonly;
  state.disabled = config.disabled;

  // Disabled: skip focus entirely, render with reduced opacity
  if (state.disabled) {
    config.opacity = std::min(config.opacity, 0.5f);
  }

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
  init_component(ctx, ep_pair, config, ComponentType::TextInput, false,
                 "text_input");

  auto base_corners = RoundedCorners(
      config.rounded_corners.value_or(ctx.theme.rounded_corners));
  auto field_size = has_label ? config.size.scale_x(0.5f) : config.size;

  // Only add rounded corners component if any corners are actually rounded
  // This respects intentional square corner designs
  if (base_corners.get().any()) {
    auto &corners_cmp =
        entity.template addComponentIfMissing<HasRoundedCorners>();
    corners_cmp.rounded_corners = base_corners.get();
    corners_cmp.roundness = config.roundness.value_or(ctx.theme.roundness);
    corners_cmp.segments = config.segments.value_or(ctx.theme.segments);
  }

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

  // Create input field container (clip overflow for horizontal scroll)
  auto field_result =
      div(ctx, mk(entity, has_label ? 1 : 0),
          ComponentConfig::inherit_from(config, "text_input_field")
              .with_size(field_size)
              .with_background(Theme::Usage::Secondary)
              .with_rounded_corners(has_label ? base_corners.left_sharp()
                                              : base_corners)
              .with_alignment(TextAlignment::Left)
              .with_overflow(Overflow::Hidden)
              .with_render_layer(config.render_layer + 1));

  auto &field_entity = field_result.ent();

  // Ensure HasLabel exists and set display text
  auto &field_label = field_entity.template addComponentIfMissing<HasLabel>();
  field_label.label = display_text;

  // The batched text renderer draws at label_rect.x directly, without
  // draw_text_in_rect's 5px internal margin. text_x_offset already
  // subtracts this value, so cursor/selection must use the same origin.
  constexpr float DRAW_TEXT_MARGIN = 5.f;

  // Derive font size and padding from field height so everything scales
  // proportionally. Uses previous frame's computed height (converges in 1 frame).
  {
    auto &field_cmp = field_entity.template get<UIComponent>();
    float field_h = field_cmp.computed[Axis::Y];
    if (field_h > 0.f) {
      float derived_fs = field_h * 0.5f;
      std::string fn = config.font_name != UIComponent::UNSET_FONT
                           ? config.font_name
                           : UIComponent::DEFAULT_FONT;
      field_cmp.enable_font(fn, pixels(derived_fs), true);

      float pad_h = field_h * 0.125f;
      float pad_w = field_h * 0.35f;
      field_cmp.set_desired_padding(Padding{.top = pixels(pad_h),
                                            .bottom = pixels(pad_h),
                                            .left = pixels(pad_w),
                                            .right = pixels(pad_w)});

      // draw_text_in_rect adds DRAW_TEXT_MARGIN internally; subtract it so
      // the total inset from the field edge equals pad_w. Apply horizontal
      // scroll offset to shift text left when overflowing.
      field_label.text_x_offset =
          pad_w - DRAW_TEXT_MARGIN - state.scroll_offset_x;
    }
  }

  // Update focus state - check if this field OR the parent container has focus
  if (!state.disabled) {
    field_entity.template addComponentIfMissing<InFocusCluster>();
  }
  bool field_has_focus = ctx.has_focus(field_entity.id);
  bool parent_has_focus = ctx.has_focus(entity.id);
  state.is_focused = !state.disabled && (field_has_focus || parent_has_focus);

  // Select all text when gaining focus via tab (not mouse click).
  // Mouse clicks position the cursor via the click listener instead.
  if (state.is_focused && !state.was_focused) {
    bool mouse_caused_focus =
        ctx.is_mouse_press(field_entity.id) || ctx.is_mouse_press(entity.id);
    if (!mouse_caused_focus && state.text_size() > 0) {
      state.selection_anchor = 0;
      state.cursor_position = state.text_size();
      reset_blink(state);
    }
  }
  state.was_focused = state.is_focused;

  if (state.is_focused) {
    auto focus_color = ctx.theme.accent;
    field_entity.template addComponentIfMissing<HasBorder>();
    field_entity.template get<HasBorder>().border =
        Border::all(focus_color, pixels(2.0f));
  }

  // Render cursor as overlay when focused
  if (state.is_focused) {
    float blink_dt = ctx.dt > 0.f ? ctx.dt : (1.f / 60.f);
    bool show_cursor = update_blink(state, blink_dt);

    auto &field_cmp = field_entity.template get<UIComponent>();
    auto font_manager = EntityHelper::get_singleton_cmp<FontManager>();

    float cursor_x = 0.f;
    float screen_height = 720.f;
    if (auto *pcr = EntityHelper::get_singleton_cmp<
            window_manager::ProvidesCurrentResolution>()) {
      screen_height = static_cast<float>(pcr->current_resolution.height);
    }

    float derived_fs = field_cmp.font_size.value;
    float uis = imm::ThemeDefaults::get().theme.ui_scale;
    float actual_font_size = resolve_to_pixels(
        pixels(derived_fs), screen_height,
        field_cmp.resolved_scaling_mode, uis);

    float pad_left = field_cmp.computed_padd[Axis::left];
    float pad_top = field_cmp.computed_padd[Axis::top];

    float cursor_height = actual_font_size;
    float cursor_y = 0.f;

    if (font_manager) {
      std::string font_name = config.font_name == UIComponent::UNSET_FONT
                                  ? UIComponent::DEFAULT_FONT
                                  : config.font_name;
      Font font = font_manager->get_font(font_name);
      font_manager->set_active(font_name);

      RectangleType field_rect = field_cmp.rect();
      const std::string &measure_str =
          display_text.empty() ? std::string(" ") : display_text;

      Vector2Type margin_px{0.f, DRAW_TEXT_MARGIN};
      TextPositionResult text_pos = position_text_ex(
          *font_manager, measure_str.c_str(), field_rect,
          TextAlignment::Left, margin_px, actual_font_size);

      cursor_height = text_pos.rect.height;
      cursor_y = text_pos.rect.y - field_rect.y - pad_top;

      // text_x_offset places the DrawTextEx origin at (pad_w - DRAW_TEXT_MARGIN)
      // from rect().x. The cursor is positioned within the content area which
      // starts at pad_w from rect().x. Shift by -DRAW_TEXT_MARGIN so cursor
      // aligns with the DrawTextEx origin where measure_text values apply.
      float text_start_x = -DRAW_TEXT_MARGIN;

      if (!display_text.empty() && display_cursor_pos > 0) {
        size_t safe_pos = std::min(display_cursor_pos, display_text.size());
        std::string text_before = display_text.substr(0, safe_pos);
        Vector2Type text_size =
            measure_text(font, text_before.c_str(), actual_font_size, 1.f);
        cursor_x = text_start_x + text_size.x;
      } else {
        cursor_x = text_start_x;
      }

      // Horizontal scroll: keep cursor visible within the content area
      float content_w = field_rect.width - pad_left -
                        field_cmp.computed_padd[Axis::right];
      if (content_w > 0.f) {
        constexpr float SCROLL_MARGIN = 4.f;
        if (cursor_x - state.scroll_offset_x > content_w - SCROLL_MARGIN)
          state.scroll_offset_x = cursor_x - content_w + SCROLL_MARGIN;
        if (cursor_x - state.scroll_offset_x < SCROLL_MARGIN)
          state.scroll_offset_x = cursor_x - SCROLL_MARGIN;
        if (state.scroll_offset_x < 0.f)
          state.scroll_offset_x = 0.f;
      }

      cursor_x -= state.scroll_offset_x;

    }

    // Selection highlight
    if (state.has_selection() && font_manager) {
      std::string font_name = config.font_name == UIComponent::UNSET_FONT
                                   ? UIComponent::DEFAULT_FONT
                                   : config.font_name;
      Font font = font_manager->get_font(font_name);

      float sel_text_x = -DRAW_TEXT_MARGIN;

      auto measure_x = [&](size_t byte_pos) -> float {
        if (byte_pos == 0 || display_text.empty()) return sel_text_x;
        size_t safe = std::min(byte_pos, display_text.size());
        std::string sub = display_text.substr(0, safe);
        Vector2Type sz = measure_text(font, sub.c_str(), actual_font_size, 1.f);
        return sel_text_x + sz.x;
      };

      size_t sel_start_byte = state.selection_start();
      size_t sel_end_byte = state.selection_end();
      if (config.mask_char) {
        // For masked text, convert byte positions to codepoint positions
        size_t cp_start = 0, cp_end = 0, cp = 0;
        for (size_t i = 0; i < state.text().size();) {
          if (i == sel_start_byte) cp_start = cp;
          if (i == sel_end_byte) cp_end = cp;
          i += utf8_char_length(state.text(), i);
          cp++;
        }
        if (sel_end_byte >= state.text().size()) cp_end = cp;
        sel_start_byte = cp_start;
        sel_end_byte = cp_end;
      }

      float sel_x1 = measure_x(sel_start_byte) - state.scroll_offset_x;
      float sel_x2 = measure_x(sel_end_byte) - state.scroll_offset_x;
      float sel_width = std::max(sel_x2 - sel_x1, 2.f);

      auto sel_color = ctx.theme.accent;
      sel_color.a = 100;
      (void)div(
          ctx, mk(field_entity, 1),
          ComponentConfig()
              .with_size(ComponentSize(pixels(sel_width), pixels(cursor_height)))
              .with_custom_background(sel_color)
              .with_absolute_position(sel_x1, cursor_y)
              .with_skip_tabbing(true)
              .with_skip_grid_snap()
              .with_debug_name("selection")
              .with_render_layer(config.render_layer + 5));
    }

    constexpr float CURSOR_WIDTH = 2.0f;

    (void)div(
        ctx, mk(field_entity, 0),
        ComponentConfig()
            .with_size(ComponentSize(pixels(CURSOR_WIDTH), pixels(cursor_height)))
            .with_custom_background(ctx.theme.font)
            .with_absolute_position(cursor_x, cursor_y)
            .with_opacity(show_cursor ? 1.0f : 0.0f)
            .with_skip_tabbing(true)
            .with_skip_grid_snap()
            .with_debug_name("cursor")
            .with_render_layer(config.render_layer + 10));
  }

  // Click to focus, position cursor, handle multi-click and shift+click
  field_entity.template addComponentIfMissing<HasClickListener>(
      [&ctx](Entity &ent) {
        if (!ent.has<HasTextInputState>()) return;
        auto &s = ent.get<HasTextInputState>();
        if (s.disabled) return;
        ctx.set_focus(ent.id);
        auto &cmp = ent.get<UIComponent>();

        bool shift = input::is_key_down(keys::LEFT_SHIFT) ||
                     input::is_key_down(keys::RIGHT_SHIFT);

        auto *fm = EntityHelper::get_singleton_cmp<FontManager>();
        if (!fm) {
          if (!shift) s.clear_selection();
          reset_blink(s);
          return;
        }

        std::string fn = cmp.font_name == UIComponent::UNSET_FONT
                             ? UIComponent::DEFAULT_FONT
                             : cmp.font_name;
        Font font = fm->get_font(fn);

        float screen_h = 720.f;
        if (auto *pcr = EntityHelper::get_singleton_cmp<
                window_manager::ProvidesCurrentResolution>())
          screen_h = static_cast<float>(pcr->current_resolution.height);

        float uis = imm::ThemeDefaults::get().theme.ui_scale;
        float afs = resolve_to_pixels(cmp.font_size, screen_h,
                                      cmp.resolved_scaling_mode, uis);

        RectangleType fr = cmp.rect();
        float pad_left = cmp.computed_padd[Axis::left];
        float local_x = ctx.mouse.pos.x - fr.x - pad_left +
                         DRAW_TEXT_MARGIN + s.scroll_offset_x;
        std::string dt = ent.has<HasLabel>() ? ent.get<HasLabel>().label
                                             : s.text();
        size_t click_pos = pixel_x_to_byte_offset(dt, local_x, font, afs);

        // Multi-click detection (double = word, triple = select all)
        constexpr float MULTI_CLICK_TIME = 0.4f;
        constexpr size_t MULTI_CLICK_DIST = 3;
        float now = static_cast<float>(afterhours::graphics::get_time());
        if (s.last_click_time >= 0.f &&
            (now - s.last_click_time) < MULTI_CLICK_TIME &&
            (click_pos > s.last_click_pos
                 ? click_pos - s.last_click_pos
                 : s.last_click_pos - click_pos) <= MULTI_CLICK_DIST) {
          s.click_count++;
        } else {
          s.click_count = 1;
        }
        s.last_click_time = now;
        s.last_click_pos = click_pos;

        if (s.click_count >= 3) {
          // Triple click: select all
          s.selection_anchor = 0;
          s.cursor_position = s.text_size();
          s.click_count = 0;
        } else if (s.click_count == 2) {
          // Double click: select word
          auto [ws, we] = select_word_at(s.text(), click_pos);
          s.selection_anchor = ws;
          s.cursor_position = we;
        } else {
          // Single click: position cursor
          if (shift) {
            if (!s.selection_anchor)
              s.selection_anchor = s.cursor_position;
          } else {
            s.clear_selection();
          }
          s.cursor_position = click_pos;
        }
        reset_blink(s);
      });

  // Handle input when focused
  if (state.is_focused) {
    bool editable = !state.readonly;

    // Undo/Redo (before clipboard/char input so Ctrl+Z doesn't insert)
    if (editable) {
      if constexpr (magic_enum::enum_contains<InputAction>("TextUndo")) {
        if (ctx.pressed(InputAction::TextUndo)) {
          state.undo();
          reset_blink(state);
        }
      }
      if constexpr (magic_enum::enum_contains<InputAction>("TextRedo")) {
        if (ctx.pressed(InputAction::TextRedo)) {
          state.redo();
          reset_blink(state);
        }
      }
    }

    // Copy is always allowed (even readonly). Cut/Paste need editable.
    if constexpr (magic_enum::enum_contains<InputAction>("TextCopy")) {
      if (ctx.pressed(InputAction::TextCopy)) {
        if (state.has_selection())
          clipboard::set_text(state.selected_text());
      }
    }
    if (editable) {
      if constexpr (magic_enum::enum_contains<InputAction>("TextCut")) {
        if (ctx.pressed(InputAction::TextCut)) {
          if (state.has_selection()) {
            state.push_undo_snapshot();
            clipboard::set_text(state.selected_text());
            delete_selection(state);
            reset_blink(state);
          }
        }
      }
      if constexpr (magic_enum::enum_contains<InputAction>("TextPaste")) {
        if (ctx.pressed(InputAction::TextPaste)) {
          state.push_undo_snapshot();
          if (state.has_selection()) delete_selection(state);
          std::string clip = clipboard::get_text();
          if (!clip.empty()) {
            for (size_t i = 0; i < clip.size();) {
              int cp = utf8_to_codepoint(clip, i);
              if (cp >= 32) insert_char(state, cp);
              size_t clen = utf8_char_length(clip, i);
              i += clen > 0 ? clen : 1;
            }
            state.clear_selection();
            reset_blink(state);
          }
        }
      }
    }

    // Select all (allowed in readonly)
    if (ctx.pressed(InputAction::TextSelectAll)) {
      state.selection_anchor = 0;
      state.cursor_position = state.text_size();
      reset_blink(state);
    }

    // Character input (replaces selection) - only if editable
    if (editable) {
      for (int key = input::get_char_pressed(); key > 0;
           key = input::get_char_pressed()) {
        state.push_undo_snapshot();
        if (state.has_selection()) delete_selection(state);
        if (insert_char(state, key)) {
          state.clear_selection();
          reset_blink(state);
        }
      }
    } else {
      // Drain the char queue so keys don't accumulate
      while (input::get_char_pressed() > 0) {}
    }

    // Shift-as-modifier: check raw shift state to extend/start selection
    // during any movement. This replaces separate Select* actions.
    bool shift_held = input::is_key_down(keys::LEFT_SHIFT) ||
                      input::is_key_down(keys::RIGHT_SHIFT);

    auto navigate = [&](auto move_fn) {
      if (shift_held) {
        if (!state.selection_anchor)
          state.selection_anchor = state.cursor_position;
      }
      move_fn();
      if (!shift_held) {
        state.clear_selection();
      }
      reset_blink(state);
    };

    // Word-level movement (Ctrl+Arrow / Alt+Arrow)
    if constexpr (magic_enum::enum_contains<InputAction>("TextWordLeft")) {
      if (ctx.pressed_or_repeat(InputAction::TextWordLeft)) {
        navigate([&] {
          if (!shift_held && state.has_selection()) {
            state.cursor_position = state.selection_start();
          } else {
            move_cursor_word_left(state);
          }
        });
      }
    }
    if constexpr (magic_enum::enum_contains<InputAction>("TextWordRight")) {
      if (ctx.pressed_or_repeat(InputAction::TextWordRight)) {
        navigate([&] {
          if (!shift_held && state.has_selection()) {
            state.cursor_position = state.selection_end();
          } else {
            move_cursor_word_right(state);
          }
        });
      }
    }

    // Character-level selection movement (Shift+Arrow via chord actions).
    // These are kept for backward compatibility; the shift_held path in
    // navigate() handles selection automatically.
    if constexpr (magic_enum::enum_contains<InputAction>("TextSelectLeft")) {
      if (ctx.pressed_or_repeat(InputAction::TextSelectLeft)) {
        navigate([&] { move_cursor_left(state); });
      }
    }
    if constexpr (magic_enum::enum_contains<InputAction>("TextSelectRight")) {
      if (ctx.pressed_or_repeat(InputAction::TextSelectRight)) {
        navigate([&] { move_cursor_right(state); });
      }
    }

    // Plain left/right movement (collapses selection when shift not held).
    if (ctx.pressed_or_repeat(InputAction::WidgetLeft)) {
      navigate([&] {
        if (!shift_held && state.has_selection()) {
          state.cursor_position = state.selection_start();
        } else {
          move_cursor_left(state);
        }
      });
    }
    if (ctx.pressed_or_repeat(InputAction::WidgetRight)) {
      navigate([&] {
        if (!shift_held && state.has_selection()) {
          state.cursor_position = state.selection_end();
        } else {
          move_cursor_right(state);
        }
      });
    }

    // Deletion (word-level and character-level) - only if editable
    if (editable) {
      if constexpr (magic_enum::enum_contains<InputAction>("TextDeleteWordBack")) {
        if (ctx.pressed_or_repeat(InputAction::TextDeleteWordBack)) {
          state.push_undo_snapshot();
          if (state.has_selection()) {
            delete_selection(state);
          } else {
            delete_word_before_cursor(state);
          }
          reset_blink(state);
        }
      }
      if constexpr (magic_enum::enum_contains<InputAction>("TextDeleteWordForward")) {
        if (ctx.pressed_or_repeat(InputAction::TextDeleteWordForward)) {
          state.push_undo_snapshot();
          if (state.has_selection()) {
            delete_selection(state);
          } else {
            delete_word_after_cursor(state);
          }
          reset_blink(state);
        }
      }

      if (ctx.pressed_or_repeat(InputAction::TextBackspace)) {
        state.push_undo_snapshot();
        if (state.has_selection()) {
          delete_selection(state);
        } else {
          delete_before_cursor(state);
        }
        reset_blink(state);
      }
      if (ctx.pressed_or_repeat(InputAction::TextDelete)) {
        state.push_undo_snapshot();
        if (state.has_selection()) {
          delete_selection(state);
        } else {
          delete_at_cursor(state);
        }
        reset_blink(state);
      }
    }

    // Home/End: Shift extends selection, plain collapses it
    if (ctx.pressed_or_repeat(InputAction::TextHome)) {
      navigate([&] { state.cursor_position = 0; });
    }
    if (ctx.pressed_or_repeat(InputAction::TextEnd)) {
      navigate([&] { state.cursor_position = state.text_size(); });
    }

    // Escape blurs the text field. Use FAKE instead of ROOT to prevent
    // try_to_grab from immediately re-focusing the first tabbable widget.
    if (ctx.pressed(InputAction::MenuBack)) {
      state.clear_selection();
      ctx.set_focus(ctx.FAKE);
    }

    // Enter/Submit
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
