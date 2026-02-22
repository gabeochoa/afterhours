
#pragma once

#include <cstdlib>
#include <cstring>
#include <string>

#include "../../developer.h"
#include "metal_state.h"

namespace afterhours {

// Font is an int (fontstash font ID). We wrap it so the type is distinct.
struct Font {
  int id = FONS_INVALID;
};

inline Font load_font_from_file(const char *file, int = 0) {
  auto *ctx = graphics::metal_detail::g_fons_ctx;
  if (!ctx) {
    log_warn("fontstash context not ready yet (load_font_from_file)");
    return Font{FONS_INVALID};
  }
  int id = fonsAddFont(ctx, file, file);
  if (id == FONS_INVALID) {
    log_warn("Failed to load font: {}", file);
  } else {
    // Track loaded fonts and set the first one as active default
    auto &md = graphics::metal_detail::g_font_ids;
    if (graphics::metal_detail::g_font_count <
        graphics::metal_detail::MAX_FONTS) {
      md[graphics::metal_detail::g_font_count++] = id;
    }
    if (graphics::metal_detail::g_active_font == FONS_INVALID) {
      graphics::metal_detail::g_active_font = id;
    }
  }
  return Font{id};
}

inline Font load_font_from_file_with_codepoints(const char *file, int *, int) {
  // fontstash loads full TTF; codepoint filtering not needed
  return load_font_from_file(file);
}

inline int *remove_duplicate_codepoints(int *, int,
                                        int *codepointsResultCount) {
  if (codepointsResultCount)
    *codepointsResultCount = 0;
  return nullptr;
}

inline Font load_font_for_string(const std::string &,
                                 const std::string &font_file, int = 96) {
  return load_font_from_file(font_file.c_str());
}

inline float measure_text_internal(const char *text, const float size) {
  auto *ctx = graphics::metal_detail::g_fons_ctx;
  if (!ctx || graphics::metal_detail::g_active_font == FONS_INVALID)
    return 0.f;
  fonsSetFont(ctx, graphics::metal_detail::g_active_font);
  // Measure at DPI-scaled size (matching draw_text) and convert back to logical pixels.
  float dpi = sapp_dpi_scale();
  fonsSetSize(ctx, size * dpi);
  fonsSetAlign(ctx, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
  return fonsTextBounds(ctx, 0, 0, text, nullptr, nullptr) / dpi;
}

inline Vector2Type measure_text(const Font font, const char *text,
                                const float size, const float /*spacing*/) {
  auto *ctx = graphics::metal_detail::g_fons_ctx;
  if (!ctx)
    return Vector2Type{0, 0};
  int fid = (font.id != FONS_INVALID) ? font.id
                                      : graphics::metal_detail::g_active_font;
  if (fid == FONS_INVALID)
    return Vector2Type{0, 0};
  fonsSetFont(ctx, fid);
  // Measure at DPI-scaled size (matching draw_text_ex) and convert back.
  float dpi = sapp_dpi_scale();
  fonsSetSize(ctx, size * dpi);
  fonsSetAlign(ctx, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
  float bounds[4] = {};
  fonsTextBounds(ctx, 0, 0, text, nullptr, bounds);
  float w = (bounds[2] - bounds[0]) / dpi;
  float ascender, descender, lineh;
  fonsVertMetrics(ctx, &ascender, &descender, &lineh);
  return Vector2Type{w, lineh / dpi};
}

inline Vector2Type measure_text_utf8(const Font font, const char *text,
                                     const float size, const float spacing) {
  return measure_text(font, text, size, spacing);
}

inline float get_first_glyph_bearing(const Font, const char *) { return 0.0f; }

} // namespace afterhours
