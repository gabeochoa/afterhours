#pragma once

#include "line_index.h"
#include <concepts>
#include <string>

namespace afterhours {
namespace text_input {

/// Concept for any text input state (single-line or multiline).
/// Used to constrain template functions for better error messages.
template <typename T>
concept AnyTextInputState = requires(T t, const T ct) {
  { t.storage };
  { t.cursor_position } -> std::convertible_to<size_t>;
  { t.changed_since } -> std::convertible_to<bool>;
  { t.max_length } -> std::convertible_to<size_t>;
  { t.cursor_blink_timer } -> std::convertible_to<float>;
  { t.cursor_blink_rate } -> std::convertible_to<float>;
  { ct.text() } -> std::convertible_to<std::string>;
  { ct.text_size() } -> std::convertible_to<size_t>;
};

/// Concept for any text area state (multiline text input).
/// Extends AnyTextInputState with multiline-specific requirements.
template <typename T>
concept AnyTextAreaState = AnyTextInputState<T> && requires(T t, const T ct) {
  { t.line_index } -> std::convertible_to<LineIndex &>;
  { t.preferred_column } -> std::convertible_to<size_t &>;
  { t.rebuild_line_index() } -> std::same_as<void>;
  { ct.cursor_position_rc() } -> std::convertible_to<LineIndex::Position>;
  { ct.line_count() } -> std::convertible_to<size_t>;
};

} // namespace text_input
} // namespace afterhours
