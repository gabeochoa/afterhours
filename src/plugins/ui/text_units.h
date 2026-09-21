#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace afterhours {
namespace ui {

enum struct TextUnit { Char, Word };

struct UnitSpan {
  size_t begin = 0;
  size_t end = 0;
  size_t size() const { return end - begin; }
};

namespace text_units_detail {

inline uint32_t decode(std::string_view s, size_t &i) {
  const unsigned char c = static_cast<unsigned char>(s[i]);
  uint32_t cp = c;
  size_t len = 1;
  if (c >= 0xF0) { cp = c & 0x07; len = 4; }
  else if (c >= 0xE0) { cp = c & 0x0F; len = 3; }
  else if (c >= 0xC0) { cp = c & 0x1F; len = 2; }
  for (size_t k = 1; k < len && i + k < s.size(); ++k)
    cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3F);
  i += len;
  return cp;
}

inline bool is_extend(uint32_t cp) {
  return (cp >= 0x0300 && cp <= 0x036F) || (cp >= 0x1AB0 && cp <= 0x1AFF) ||
         (cp >= 0x1DC0 && cp <= 0x1DFF) || (cp >= 0x20D0 && cp <= 0x20FF) ||
         (cp >= 0xFE00 && cp <= 0xFE0F) || (cp >= 0xFE20 && cp <= 0xFE2F) ||
         (cp >= 0x1F3FB && cp <= 0x1F3FF) || (cp >= 0xE0100 && cp <= 0xE01EF) ||
         cp == 0x200D;
}

inline bool is_regional(uint32_t cp) { return cp >= 0x1F1E6 && cp <= 0x1F1FF; }
inline bool is_space(uint32_t cp) {
  return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == 0x00A0;
}

} // namespace text_units_detail

inline void split_graphemes(std::string_view text, std::vector<UnitSpan> &out) {
  using namespace text_units_detail;
  out.clear();
  size_t i = 0;
  while (i < text.size()) {
    const size_t begin = i;
    uint32_t cp = decode(text, i);
    bool prev_zwj = false;
    int regional = is_regional(cp) ? 1 : 0;
    while (i < text.size()) {
      size_t peek = i;
      const uint32_t next = decode(text, peek);
      if (regional == 1 && is_regional(next)) {
        regional = 2;
        i = peek;
        continue;
      }
      if (is_extend(next) || prev_zwj) {
        prev_zwj = next == 0x200D;
        i = peek;
        continue;
      }
      break;
    }
    out.push_back({begin, i});
  }
}

inline void split_words(std::string_view text, std::vector<UnitSpan> &out) {
  using namespace text_units_detail;
  out.clear();
  size_t i = 0;
  while (i < text.size()) {
    size_t peek = i;
    if (is_space(decode(text, peek))) {
      i = peek;
      continue;
    }
    const size_t begin = i;
    while (i < text.size()) {
      peek = i;
      if (is_space(decode(text, peek)))
        break;
      i = peek;
    }
    out.push_back({begin, i});
  }
}

struct TextUnitCache {
  size_t hash = 0;
  TextUnit unit = TextUnit::Char;
  bool primed = false;
  std::vector<UnitSpan> spans;

  const std::vector<UnitSpan> &get(std::string_view text, TextUnit want) {
    const size_t h = std::hash<std::string_view>{}(text);
    if (!primed || h != hash || want != unit) {
      primed = true;
      hash = h;
      unit = want;
      if (want == TextUnit::Char)
        split_graphemes(text, spans);
      else
        split_words(text, spans);
    }
    return spans;
  }
};

} // namespace ui
} // namespace afterhours
