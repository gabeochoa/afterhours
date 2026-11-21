
#pragma once

#include <cstdlib>
#include <functional>
#include <cstring>
#include <string>

#include "../../developer.h"
#include "../../warn_once.h"

namespace afterhours {

using Font = FontType;

inline Font load_font_from_file(const char *, int = 0) {
  warn_once(0, "@notimplemented load_font_from_file on the none backend; "
               "returning an empty font");
  return Font();
}

inline Font load_font_from_file_with_codepoints(const char *, int *, int,
                                                int = 32) {
  log_warn("Codepoint-based font loading not supported without a backend");
  return Font();
}

inline Font load_font_from_memory(const unsigned char *, int, const char *,
                                  int = 0) {
  warn_once(0, "@notimplemented load_font_from_memory on the none backend; "
               "returning an empty font");
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
  warn_once(0, "@notimplemented load_font_for_string on the none backend; "
               "returning an empty font");
  return Font();
}
// Text measurement seam.
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

inline float text_ascent(const Font, const float size) {
  return size > 0.f ? size * 0.8f : 0.f;
}

inline float get_first_glyph_bearing(const Font, const char *) { return 0.0f; }
inline bool is_font_loaded(const Font &) { return false; }
inline bool font_has_glyph(const Font, int) { return false; }

} // namespace afterhours
