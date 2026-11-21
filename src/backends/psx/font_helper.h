
#pragma once

#include <cstdlib>
#include <functional>
#include <cstring>
#include <string>

#include "../../developer.h"
#include "../../warn_once.h"
#include "budget.h"

namespace afterhours {

using Font = FontType;

inline void charge_font_atlas(const char *what) {
  psx::budget::alloc_texture(256, 256, what);
}

inline Font load_font_from_file(const char *path, int = 0) {
  charge_font_atlas(path ? path : "font atlas");
  return Font();
}
inline Font load_font_from_file_with_codepoints(const char *path, int *, int,
                                                int = 32) {
  log_warn("Codepoint-based font loading not supported without a backend");
  charge_font_atlas(path ? path : "font atlas");
  return Font();
}
inline Font load_font_from_memory(const unsigned char *, int, const char *,
                                  int = 0) {
  charge_font_atlas("font atlas (memory)");
  return Font();
}

inline int *remove_duplicate_codepoints(int *, int,
                                        int *codepointsResultCount) {
  if (codepointsResultCount)
    *codepointsResultCount = 0;
  return nullptr;
}

inline void unload_font(Font &font) { font = Font(); }

inline Font load_font_for_string(const std::string &, const std::string &,
                                 int = 96) {
  charge_font_atlas("font atlas (string)");
  return Font();
}
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
  warn_once(0, "no backend to measure text, so it is estimated at 0.5x its "
               "height per byte, the same estimate raylib uses headless. "
               "Install set_measure_text_fn() for real widths.");
  const size_t len = text ? std::strlen(text) : 0;
  return Vector2Type{static_cast<float>(len) * size * 0.5f, size};
}
inline Vector2Type measure_text_utf8(const Font f, const char *text,
                                     const float size, const float spacing) {
  return measure_text(f, text, size, spacing);
}

inline float monospace_cell_advance(const Font font, const float size) {
  if (!std::isfinite(size) || size <= 0.f)
    return 0.f;
  return measure_text(font, "0", size, 1.f).x;
}

inline float get_first_glyph_bearing(const Font, const char *) { return 0.0f; }
inline bool is_font_loaded(const Font &) { return false; }
inline bool font_has_glyph(const Font, int) { return false; }

} // namespace afterhours
