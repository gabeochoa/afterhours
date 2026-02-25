#pragma once

#include "../../clipboard.h"
#include "../../input_system.h"
#include "../../../core/key_codes.h"
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

  // Derive font size from field height so text auto-sizes to ~50% of field.
  {
    auto &field_cmp = field_entity.template get<UIComponent>();
    float field_h = field_cmp.computed[Axis::Y];
    float derived_fs = field_h * 0.5f;
    std::string fn = config.font_name != UIComponent::UNSET_FONT
                         ? config.font_name
                         : UIComponent::DEFAULT_FONT;
    field_cmp.enable_font(fn, pixels(derived_fs), true);
  }

  // Update focus state - check if this field OR the parent container has focus
  field_entity.template addComponentIfMissing<InFocusCluster>();
  bool field_has_focus = ctx.has_focus(field_entity.id);
  bool parent_has_focus = ctx.has_focus(entity.id);
  state.is_focused = field_has_focus || parent_has_focus;

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

    constexpr float TEXT_MARGIN = 5.f;

    if (font_manager) {
      std::string font_name = config.font_name == UIComponent::UNSET_FONT
                                  ? UIComponent::DEFAULT_FONT
                                  : config.font_name;
      Font font = font_manager->get_font(font_name);
      font_manager->set_active(font_name);

      RectangleType field_rect = field_cmp.rect();
      const std::string &measure_str =
          display_text.empty() ? std::string(" ") : display_text;

      // Y margin affects vertical centering bounds; X is zero because the
      // batched renderer draws Left-aligned text at rect.x (no X margin).
      Vector2Type margin_px{0.f, TEXT_MARGIN};
      TextPositionResult text_pos = position_text_ex(
          *font_manager, measure_str.c_str(), field_rect,
          TextAlignment::Left, margin_px, actual_font_size);

      cursor_height = text_pos.rect.height;
      cursor_y = text_pos.rect.y - field_rect.y - pad_top;

      // X: batched render_text draws at rect.x; in content-area coords
      // that's -pad_left. Cursor at position 0 sits at the text origin.
      float text_start_x = -pad_left;

      if (!display_text.empty() && display_cursor_pos > 0) {
        size_t safe_pos = std::min(display_cursor_pos, display_text.size());
        std::string text_before = display_text.substr(0, safe_pos);
        Vector2Type text_size =
            measure_text(font, text_before.c_str(), actual_font_size, 1.f);
        cursor_x = text_start_x + text_size.x;
      } else {
        cursor_x = text_start_x;
      }

    }

    // Selection highlight
    if (state.has_selection() && font_manager) {
      std::string font_name = config.font_name == UIComponent::UNSET_FONT
                                   ? UIComponent::DEFAULT_FONT
                                   : config.font_name;
      Font font = font_manager->get_font(font_name);

      float sel_text_x = -pad_left;

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

      float sel_x1 = measure_x(sel_start_byte);
      float sel_x2 = measure_x(sel_end_byte);
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

  // Click to focus and collapse selection
  field_entity.template addComponentIfMissing<HasClickListener>(
      [&ctx](Entity &ent) {
        ctx.set_focus(ent.id);
        if (ent.has<HasTextInputState>()) {
          auto &s = ent.get<HasTextInputState>();
          s.clear_selection();
          reset_blink(s);
        }
      });

  // TODO: Implement horizontal scrolling when text exceeds field width

  // Handle input when focused
  if (state.is_focused) {
    // Clipboard operations (before char input so Ctrl+C/V/X don't insert)
    if constexpr (magic_enum::enum_contains<InputAction>("TextCopy")) {
      if (ctx.pressed(InputAction::TextCopy)) {
        if (state.has_selection())
          clipboard::set_text(state.selected_text());
      }
    }
    if constexpr (magic_enum::enum_contains<InputAction>("TextCut")) {
      if (ctx.pressed(InputAction::TextCut)) {
        if (state.has_selection()) {
          clipboard::set_text(state.selected_text());
          delete_selection(state);
          reset_blink(state);
        }
      }
    }
    if constexpr (magic_enum::enum_contains<InputAction>("TextPaste")) {
      if (ctx.pressed(InputAction::TextPaste)) {
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

    // Select all
    if (ctx.pressed(InputAction::TextSelectAll)) {
      state.selection_anchor = 0;
      state.cursor_position = state.text_size();
      reset_blink(state);
    }

    // Character input (replaces selection)
    for (int key = input::get_char_pressed(); key > 0;
         key = input::get_char_pressed()) {
      if (state.has_selection()) delete_selection(state);
      if (insert_char(state, key)) {
        state.clear_selection();
        reset_blink(state);
      }
    }

    // Selection movement (Shift+Arrow via chord actions)
    if constexpr (magic_enum::enum_contains<InputAction>("TextSelectLeft")) {
      if (ctx.pressed(InputAction::TextSelectLeft)) {
        if (!state.selection_anchor)
          state.selection_anchor = state.cursor_position;
        move_cursor_left(state);
        reset_blink(state);
      }
    }
    if constexpr (magic_enum::enum_contains<InputAction>("TextSelectRight")) {
      if (ctx.pressed(InputAction::TextSelectRight)) {
        if (!state.selection_anchor)
          state.selection_anchor = state.cursor_position;
        move_cursor_right(state);
        reset_blink(state);
      }
    }

    // Plain movement (collapses selection).
    // suppress_permissive_duplicates ensures bare WidgetLeft/Right are removed
    // from inputs_pressed when a chord (e.g. Shift+Left = TextSelectLeft)
    // claims the same key, so ctx.pressed() is safe here.
    if (ctx.pressed(InputAction::WidgetLeft)) {
      if (state.has_selection()) {
        state.cursor_position = state.selection_start();
        state.clear_selection();
      } else {
        move_cursor_left(state);
      }
      reset_blink(state);
    }
    if (ctx.pressed(InputAction::WidgetRight)) {
      if (state.has_selection()) {
        state.cursor_position = state.selection_end();
        state.clear_selection();
      } else {
        move_cursor_right(state);
      }
      reset_blink(state);
    }

    if (ctx.pressed(InputAction::TextBackspace)) {
      if (state.has_selection()) {
        delete_selection(state);
      } else {
        delete_before_cursor(state);
      }
      reset_blink(state);
    }
    if (ctx.pressed(InputAction::TextDelete)) {
      if (state.has_selection()) {
        delete_selection(state);
      } else {
        delete_at_cursor(state);
      }
      reset_blink(state);
    }

    // Home/End collapse selection
    if (ctx.pressed(InputAction::TextHome)) {
      state.cursor_position = 0;
      state.clear_selection();
      reset_blink(state);
    }
    if (ctx.pressed(InputAction::TextEnd)) {
      state.cursor_position = state.text_size();
      state.clear_selection();
      reset_blink(state);
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
