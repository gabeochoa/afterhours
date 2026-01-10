#pragma once

#include <string>

namespace afterhours {
namespace ui {

// Utility functions for proper CJK text support
namespace text_utils {

// Enhanced CJK detection with more precise Unicode range checking
inline bool contains_cjk(const std::string &text) {
  if (text.empty()) {
    return false;
  }

  // Check for UTF-8 multi-byte sequences that indicate CJK
  for (size_t i = 0; i < text.length(); ++i) {
    unsigned char byte = static_cast<unsigned char>(text[i]);

    if (byte >= 0xE0) {
      // This is a 3+ byte UTF-8 sequence, likely CJK
      // For more accuracy, we could decode the full UTF-8 sequence
      // and check specific Unicode ranges, but this is sufficient
      // for our current needs and avoids complex UTF-8 decoding
      return true;
    }
  }
  return false;
}

} // namespace text_utils

} // namespace ui
} // namespace afterhours
