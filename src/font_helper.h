
#pragma once

#include "developer.h"

namespace afterhours {
#ifdef AFTER_HOURS_USE_RAYLIB
using Font = raylib::Font;
inline raylib::Font load_font_from_file(const char *file) {
  return raylib::LoadFont(file);
}

// Add codepoint-based font loading for CJK support
inline raylib::Font load_font_from_file_with_codepoints(const char *file,
                                                        int *codepoints,
                                                        int codepoint_count) {
  if (!file || !codepoints || codepoint_count <= 0) {
    log_error(
        "Invalid parameters for font loading: file={}, codepoints={}, count={}",
        file ? file : "null", codepoints ? "valid" : "null", codepoint_count);
    return raylib::GetFontDefault();
  }
  return raylib::LoadFontEx(file, 32, codepoints, codepoint_count);
}

inline float measure_text_internal(const char *content, const float size) {
  return raylib::MeasureText(content, size);
}
inline vec2 measure_text(const raylib::Font font, const char *content,
                         const float size, const float spacing) {
  return raylib::MeasureTextEx(font, content, size, spacing);
}

// Add proper UTF-8 text measurement for CJK support
inline vec2 measure_text_utf8(const raylib::Font font, const char *content,
                              const float size, const float spacing) {
  if (!content) {
    log_warn("Null content passed to measure_text_utf8");
    return vec2{0, 0};
  }

  if (size <= 0) {
    log_warn("Invalid font size {} passed to measure_text_utf8", size);
    return vec2{0, 0};
  }

  // Use the existing measure_text for now, but this could be enhanced
  // to handle CJK text better with proper glyph spacing
  return raylib::MeasureTextEx(font, content, size, spacing);
}
#else
using Font = FontType;
inline Font load_font_from_file(const char *) { return Font(); }
inline Font load_font_from_file_with_codepoints(const char *, int *, int) {
  log_warn("Codepoint-based font loading not supported without raylib");
  return Font();
}
inline float measure_text_internal(const char *, const float) {
  log_warn("Text size measuring not supported. Either use "
           "AFTER_HOURS_USE_RAYLIB or provide your own through "
           "set_measure_text_fn()");
  return 0.f;
}
inline Vector2Type measure_text(const Font, const char *, const float /*size*/,
                                const float /*spacing*/) {
  log_warn("Text size measuring not supported. Either use "
           "AFTER_HOURS_USE_RAYLIB or provide your own through "
           "set_measure_text_fn()");
  return Vector2Type{0, 0};
}
inline Vector2Type measure_text_utf8(const Font, const char *,
                                     const float /*size*/,
                                     const float /*spacing*/) {
  log_warn("UTF-8 text measuring not supported. Either use "
           "AFTER_HOURS_USE_RAYLIB or provide your own through "
           "set_measure_text_fn()");
  return Vector2Type{0, 0};
}
#endif
} // namespace afterhours
