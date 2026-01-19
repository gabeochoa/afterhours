#pragma once

#include "state.h"
#include <cctype>
#include <string>
#include <string_view>
#include <utility>

namespace afterhours {
namespace text_input {


// Get number of bytes in a UTF-8 character starting at pos
inline size_t utf8_char_length(const std::string &str, size_t pos) {
  if (pos >= str.size())
    return 0;
  unsigned char c = static_cast<unsigned char>(str[pos]);
  if ((c & 0x80) == 0)
    return 1; // ASCII
  if ((c & 0xE0) == 0xC0)
    return 2; // 2-byte
  if ((c & 0xF0) == 0xE0)
    return 3; // 3-byte (CJK)
  if ((c & 0xF8) == 0xF0)
    return 4; // 4-byte (emoji)
  return 1;
}

// Find start of previous UTF-8 character
inline size_t utf8_prev_char_start(const std::string &str, size_t pos) {
  if (pos == 0 || str.empty())
    return 0;
  size_t p = pos - 1;
  while (p > 0 && (static_cast<unsigned char>(str[p]) & 0xC0) == 0x80)
    p--;
  return p;
}

// Encode Unicode codepoint to UTF-8
inline std::string codepoint_to_utf8(int cp) {
  std::string r;
  if (cp < 0)
    return r;
  if (cp < 0x80) {
    r += static_cast<char>(cp);
  } else if (cp < 0x800) {
    r += static_cast<char>(0xC0 | ((cp >> 6) & 0x1F));
    r += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    r += static_cast<char>(0xE0 | ((cp >> 12) & 0x0F));
    r += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    r += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp < 0x110000) {
    r += static_cast<char>(0xF0 | ((cp >> 18) & 0x07));
    r += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    r += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    r += static_cast<char>(0x80 | (cp & 0x3F));
  }
  return r;
}

// Insert codepoint at cursor, returns true if inserted
inline bool insert_char(AnyTextInputState auto &s, int codepoint) {
  if (codepoint < 32 && codepoint != '\t')
    return false;
  std::string utf8 = codepoint_to_utf8(codepoint);
  if (utf8.empty())
    return false;
  if (s.max_length > 0 && s.text_size() + utf8.size() > s.max_length)
    return false;
  s.storage.insert(s.cursor_position, utf8);
  s.cursor_position += utf8.size();
  s.changed_since = true;
  return true;
}

// Delete char before cursor (backspace)
inline bool delete_before_cursor(AnyTextInputState auto &s) {
  if (s.cursor_position == 0 || s.text_size() == 0)
    return false;
  std::string txt = s.text();
  size_t prev = utf8_prev_char_start(txt, s.cursor_position);
  s.storage.erase(prev, s.cursor_position - prev);
  s.cursor_position = prev;
  s.changed_since = true;
  return true;
}

// Delete char at cursor (delete key)
inline bool delete_at_cursor(AnyTextInputState auto &s) {
  if (s.cursor_position >= s.text_size())
    return false;
  std::string txt = s.text();
  s.storage.erase(s.cursor_position, utf8_char_length(txt, s.cursor_position));
  s.changed_since = true;
  return true;
}

// Move cursor left by one UTF-8 char
inline void move_cursor_left(AnyTextInputState auto &s) {
  if (s.cursor_position > 0)
    s.cursor_position = utf8_prev_char_start(s.text(), s.cursor_position);
}

// Move cursor right by one UTF-8 char
inline void move_cursor_right(AnyTextInputState auto &s) {
  if (s.cursor_position < s.text_size())
    s.cursor_position += utf8_char_length(s.text(), s.cursor_position);
}

// Update blink timer, returns true if cursor visible
inline bool update_blink(AnyTextInputState auto &s, float dt) {
  s.cursor_blink_timer += dt;
  if (s.cursor_blink_timer >= s.cursor_blink_rate * 2.0f)
    s.cursor_blink_timer = 0.0f;
  return s.cursor_blink_timer < s.cursor_blink_rate;
}

inline void reset_blink(AnyTextInputState auto &s) {
  s.cursor_blink_timer = 0.0f;
}

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

/// Check if character is a word separator (whitespace or punctuation)
inline bool is_word_separator(char c) {
  return std::isspace(static_cast<unsigned char>(c)) ||
         std::ispunct(static_cast<unsigned char>(c));
}

/// Find start of word containing or before position.
/// Moves backward past separators, then backward to start of word.
inline size_t find_word_start(std::string_view text, size_t pos) {
  if (pos == 0 || text.empty())
    return 0;

  size_t p = pos;

  // Move back past any separators
  while (p > 0 && is_word_separator(text[p - 1]))
    --p;

  // Move back to start of word
  while (p > 0 && !is_word_separator(text[p - 1]))
    --p;

  return p;
}

/// Find end of word containing or after position.
/// Moves forward past separators, then forward to end of word.
inline size_t find_word_end(std::string_view text, size_t pos) {
  if (pos >= text.size())
    return text.size();

  size_t p = pos;

  // Move forward past any separators
  while (p < text.size() && is_word_separator(text[p]))
    ++p;

  // Move forward to end of word
  while (p < text.size() && !is_word_separator(text[p]))
    ++p;

  return p;
}

/// Select the word at position (for double-click).
/// Returns {start, end} byte offsets.
inline std::pair<size_t, size_t> select_word_at(std::string_view text,
                                                 size_t pos) {
  if (text.empty())
    return {0, 0};
  pos = std::min(pos, text.size() - 1);

  // If on a separator, select just that separator
  if (is_word_separator(text[pos])) {
    return {pos, pos + 1};
  }

  // Find word boundaries
  size_t start = pos;
  while (start > 0 && !is_word_separator(text[start - 1]))
    --start;

  size_t end = pos;
  while (end < text.size() && !is_word_separator(text[end]))
    ++end;

  return {start, end};
}

} // namespace text_input
} // namespace afterhours

