#pragma once

#include <cstddef>
#include <string>

#include "../../core/key_codes.h"

namespace afterhours {
namespace ui {

struct ParsedAccessKey {
  std::string display;
  int underline_index = -1;
  char key = 0;
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
      out.display += '&';
      i++;
      continue;
    }
    out.display += next;
    i += 2;
  }
  return out;
}

inline int key_code_for_access_char(char c) {
  if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
  if (c >= 'A' && c <= 'Z')
    return keys::A + (c - 'A');
  if (c >= '0' && c <= '9')
    return keys::ZERO + (c - '0');
  return 0;
}

}
}
