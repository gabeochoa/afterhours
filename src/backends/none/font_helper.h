
#pragma once

#include <cstdlib>
#include <cstring>
#include <string>

#include "../../developer.h"

namespace afterhours {

using Font = FontType;

inline Font load_font_from_file(const char *) { return Font(); }
inline Font load_font_from_file_with_codepoints(const char *, int *, int) {
  log_warn("Codepoint-based font loading not supported without a backend");
  return Font();
}

inline int *remove_duplicate_codepoints(int *, int,
                                        int *codepointsResultCount) {
  if (codepointsResultCount) *codepointsResultCount = 0;
  return nullptr;
}

inline Font load_font_for_string(const std::string &, const std::string &,
                                 int = 96) {
  return Font();
}
inline float measure_text_internal(const char *, const float) {
  log_warn("Text size measuring not supported. Either use "
           "AFTER_HOURS_USE_RAYLIB or provide your own through "
           "set_measure_text_fn()");
  return 0.f;
}
inline Vector2Type measure_text(const Font, const char *, const float,
                                const float) {
  log_warn("Text size measuring not supported. Either use "
           "AFTER_HOURS_USE_RAYLIB or provide your own through "
           "set_measure_text_fn()");
  return Vector2Type{0, 0};
}
inline Vector2Type measure_text_utf8(const Font, const char *, const float,
                                     const float) {
  return Vector2Type{0, 0};
}

inline float get_first_glyph_bearing(const Font, const char *) {
  return 0.0f;
}

} // namespace afterhours
