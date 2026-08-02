
#pragma once

#include <cstdlib>
#include <functional>
#include <cstring>
#include <string>

#include "../../developer.h"

namespace afterhours {

using Font = FontType;

inline Font load_font_from_file(const char *, int = 0) { return Font(); }
inline Font load_font_from_file_with_codepoints(const char *, int *, int) {
  log_warn("Codepoint-based font loading not supported without a backend");
  return Font();
}

inline int *remove_duplicate_codepoints(int *, int,
                                        int *codepointsResultCount) {
  if (codepointsResultCount)
    *codepointsResultCount = 0;
  return nullptr;
}

inline Font load_font_for_string(const std::string &, const std::string &,
                                 int = 96) {
  return Font();
}
// Text measurement seam.
//
// The warnings below have always told callers to "provide your own through
// set_measure_text_fn()", but no such setter existed for this backend -- so
// measure_text returned {0,0} and anything that branches on measured width
// silently took the "it fits" path. That is why the renderer's ellipsis
// truncation could never be tested: text_size.x of 0 is <= any max_width, so
// the truncation code returned early every time.
//
// AutoLayout has its own hook (set_measure_text_fn on the layout object) but
// the renderer calls this free function directly, so it needs its own.
using MeasureTextFn =
    std::function<Vector2Type(const char *, float /*size*/, float /*spacing*/)>;

inline MeasureTextFn &measure_text_fn() {
  static MeasureTextFn fn;
  return fn;
}

inline void set_measure_text_fn(MeasureTextFn fn) {
  measure_text_fn() = std::move(fn);
}

inline Vector2Type measure_text(const Font, const char *text, const float size,
                                const float spacing) {
  if (measure_text_fn())
    return measure_text_fn()(text ? text : "", size, spacing);
  log_warn("Text size measuring not supported. Either use "
           "AFTER_HOURS_USE_RAYLIB or provide your own through "
           "set_measure_text_fn()");
  return Vector2Type{0, 0};
}
inline float measure_text_internal(const char *text, const float size) {
  if (measure_text_fn())
    return measure_text_fn()(text ? text : "", size, 1.f).x;
  log_warn("Text size measuring not supported. Either use "
           "AFTER_HOURS_USE_RAYLIB or provide your own through "
           "set_measure_text_fn()");
  return 0.f;
}
inline Vector2Type measure_text_utf8(const Font f, const char *text,
                                     const float size, const float spacing) {
  return measure_text(f, text, size, spacing);
}

inline float get_first_glyph_bearing(const Font, const char *) { return 0.0f; }
inline bool is_font_loaded(const Font &) { return false; }

} // namespace afterhours
