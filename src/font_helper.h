
#pragma once

#include "developer.h"

namespace afterhours {
#ifdef AFTER_HOURS_USE_RAYLIB
using Font = raylib::Font;
inline raylib::Font load_font_from_file(const char *file) {
  return raylib::LoadFont(file);
}
inline float measure_text_internal(const char *content, float size) {
  return raylib::MeasureText(content, size);
}
inline vec2 measure_text(raylib::Font font, const char *content, float size,
                         float spacing) {
  return raylib::MeasureTextEx(font, content, size, spacing);
}
#else
using Font = FontType;
inline Font load_font_from_file(const char *) { return Font(); }
inline float measure_text_internal(const char *, float) {
  log_warn("Text size measuring not supported. Either use "
           "AFTER_HOURS_USE_RAYLIB or provide your own through "
           "set_measure_text_fn()");
  return 0.f;
}
inline Vector2Type measure_text(Font, const char *, float /*size*/,
                                float /*spacing*/) {
  log_warn("Text size measuring not supported. Either use "
           "AFTER_HOURS_USE_RAYLIB or provide your own through "
           "set_measure_text_fn()");
  return Vector2Type{0, 0};
}
#endif
} // namespace afterhours
