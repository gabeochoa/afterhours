#pragma once

#include "line_index.h"
#include "selection.h"
#include "state.h"
#include "../../../core/system.h"

namespace afterhours {
namespace text_input {

/// Updates cursor blink timer for all text input states.
/// Run this each frame to animate cursor visibility.
struct TextCursorBlinkSystem : System<HasTextInputState> {
  void for_each_with(Entity &, HasTextInputState &state, float dt) override {
    if (!state.is_focused)
      return;

    state.cursor_blink_timer += dt;
    if (state.cursor_blink_timer >= state.cursor_blink_rate * 2.0f) {
      state.cursor_blink_timer = 0.0f;
    }
  }
};

/// Rebuilds LineIndex when text content changes.
/// Uses hash comparison to avoid unnecessary rebuilds.
struct LineIndexUpdateSystem : System<HasLineIndex, HasTextInputState> {
  void for_each_with(Entity &, HasLineIndex &line_index,
                     HasTextInputState &state, float) override {
    std::string text = state.text();
    
    size_t current_hash = 0;
    for (char c : text) {
      current_hash = current_hash * 31 + static_cast<size_t>(c);
    }
    current_hash ^= text.size();

    if (current_hash != line_index.last_text_hash) {
      line_index.index.rebuild(text);
      line_index.last_text_hash = current_hash;
    }
  }
};

/// Processes selection state based on input.
/// This system handles shift+arrow key selection extension.
///
/// Note: This is a base system - applications can extend or replace it
/// for custom selection behavior (e.g., word selection, block selection).
struct TextSelectionSystem : System<HasTextSelection, HasTextInputState> {
  void for_each_with(Entity &, HasTextSelection &selection,
                     HasTextInputState &state, float) override {
    // Keep selection cursor in sync with text input cursor
    // when there's no active selection
    if (!selection.selection.has_selection()) {
      selection.selection.cursor = state.cursor_position;
      selection.selection.anchor = state.cursor_position;
    }
  }
};

/// Clamps cursor position to valid range for multi-line text.
struct CursorClampSystem : System<HasLineIndex, HasTextInputState> {
  void for_each_with(Entity &, HasLineIndex &,
                     HasTextInputState &state, float) override {
    if (state.cursor_position > state.text_size()) {
      state.cursor_position = state.text_size();
    }
  }
};

} // namespace text_input
} // namespace afterhours

