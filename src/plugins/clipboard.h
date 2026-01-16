#pragma once

#include <string>
#include <string_view>

namespace afterhours {
namespace clipboard {

#ifdef AFTER_HOURS_USE_RAYLIB

// Set the system clipboard to the specified UTF-8 text
inline void set_text(std::string_view text) {
  // raylib's SetClipboardText expects a null-terminated C string
  std::string str(text);
  raylib::SetClipboardText(str.c_str());
}

// Get the current clipboard contents as a UTF-8 string
// Returns empty string if clipboard is empty or doesn't contain text
inline std::string get_text() {
  const char *text = raylib::GetClipboardText();
  if (text == nullptr) {
    return "";
  }
  return std::string(text);
}

// Check if the clipboard contains text
inline bool has_text() {
  const char *text = raylib::GetClipboardText();
  return text != nullptr && text[0] != '\0';
}

#else

// Fallback implementations when raylib is not available
inline void set_text(std::string_view) {
  // No-op without a backend
}

inline std::string get_text() { return ""; }

inline bool has_text() { return false; }

#endif

} // namespace clipboard
} // namespace afterhours

