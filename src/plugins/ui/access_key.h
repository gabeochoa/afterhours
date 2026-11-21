#pragma once

// Alt-style access keys: a label may mark its mnemonic with '&' before the
// access char (e.g. "&File" means press Alt+F). These helpers parse that
// marker into the display text plus the byte index of the underlined glyph,
// and map the access char to a key code for Alt+char activation. Only the
// first marker counts; "&&" is an escaped literal '&', and a trailing '&'
// is literal (it has no next char to mark).

#include <cstddef>
#include <string>

#include "../../core/key_codes.h"

namespace afterhours {
namespace ui {

struct ParsedAccessKey {
  std::string display; // label with the marker stripped
  int underline_index = -1; // byte index into display; -1 = none
  char key = 0; // access char (0 when none)
  bool has_access = false;
};

inline ParsedAccessKey parse_access_key(const std::string &label) {
  ParsedAccessKey out;
  for (size_t i = 0; i < label.size();) {
    const char c = label[i];
    if (c != '&' || i + 1 >= label.size()) {
      out.display += c;
      i++;
      continue;
    }
    const char next = label[i + 1];
    if (next == '&') {
      out.display += '&';
      i += 2;
      continue;
    }
    if (!out.has_access) {
      out.has_access = true;
      out.key = next;
      out.underline_index = static_cast<int>(out.display.size());
    } else {
      // Later markers stay literal so "A&B&C" renders as written.
      out.display += '&';
      i++;
      continue;
    }
    out.display += next;
    i += 2;
  }
  return out;
}

// Key code an Alt+char press must match against, 0 when the char has no key
// (anything outside ASCII letters/digits cannot be pressed this way).
inline int key_code_for_access_char(char c) {
  if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
  if (c >= 'A' && c <= 'Z')
    return keys::A + (c - 'A');
  if (c >= '0' && c <= '9')
    return keys::ZERO + (c - '0');
  return 0;
}

} // namespace ui
} // namespace afterhours
