#pragma once

#include "../../../ecs.h"
#include "concepts.h"
#include "storage.h"
#include <functional>
#include <string>

namespace afterhours {
namespace text_input {

// Text input state - templated on storage backend
// Use HasTextInputState for default std::string storage
// Use HasTextInputStateT<YourStorage> for custom backends (gap buffer, rope)
template <TextStorage Storage = StringStorage>
struct HasTextInputStateT : BaseComponent {
  Storage storage;
  size_t cursor_position = 0; // Byte position in UTF-8 string
  bool changed_since = false;
  bool is_focused = false;
  size_t max_length = 256; // Maximum text length in bytes (0 = unlimited)
  float cursor_blink_timer = 0.f;  // Current timer value
  float cursor_blink_rate = 0.53f; // Seconds per half-cycle (configurable)

  HasTextInputStateT() = default;
  explicit HasTextInputStateT(const std::string &initial_text,
                              size_t max_len = 256, float blink_rate = 0.53f)
      : storage(initial_text), cursor_position(initial_text.size()),
        max_length(max_len), cursor_blink_rate(blink_rate) {}

  // Convenience accessors
  std::string text() const { return storage.str(); }
  size_t text_size() const { return storage.size(); }
};

// Default alias for simple std::string-based text input
using HasTextInputState = HasTextInputStateT<StringStorage>;

// Listener for text input events (character typing)
struct HasTextInputListener : BaseComponent {
  std::function<void(Entity &, const std::string &)> on_change;
  std::function<void(Entity &)> on_submit; // Called on Enter key

  HasTextInputListener(
      std::function<void(Entity &, const std::string &)> change_cb = nullptr,
      std::function<void(Entity &)> submit_cb = nullptr)
      : on_change(std::move(change_cb)), on_submit(std::move(submit_cb)) {}
};

} // namespace text_input
} // namespace afterhours
