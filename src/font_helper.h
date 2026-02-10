
#pragma once

#include <cstdlib>
#include <cstring>
#include <string>

#include "developer.h"

#ifdef AFTER_HOURS_USE_METAL
// Shared Metal state (fontstash context, font IDs, etc.)
#include "graphics/metal_state.h"
#endif

namespace afterhours {
#ifdef AFTER_HOURS_USE_RAYLIB
using Font = raylib::Font;
using vec2 = raylib::Vector2;
inline raylib::Font load_font_from_file(const char *file) {
  raylib::Font font = raylib::LoadFont(file);
  raylib::SetTextureFilter(font.texture, raylib::TEXTURE_FILTER_BILINEAR);
  return font;
}

// Add codepoint-based font loading for CJK support
inline raylib::Font load_font_from_file_with_codepoints(const char *file,
                                                        int *codepoints,
                                                        int codepoint_count) {
  if (!file || !codepoints || codepoint_count <= 0) {
    log_error("Invalid parameters for font loading: file={}, codepoints={}, "
              "count={}",
              file ? file : "null", codepoints ? "valid" : "null",
              codepoint_count);
    return raylib::GetFontDefault();
  }
  raylib::Font font = raylib::LoadFontEx(file, 32, codepoints, codepoint_count);
  raylib::SetTextureFilter(font.texture, raylib::TEXTURE_FILTER_BILINEAR);
  return font;
}

// Utility function to remove duplicate codepoints from an array
// Returns a newly allocated array that must be freed by the caller
// The result count is written to codepointsResultCount
inline int *remove_duplicate_codepoints(int *codepoints, int codepointCount,
                                        int *codepointsResultCount) {
  if (!codepoints || codepointCount <= 0 || !codepointsResultCount) {
    if (codepointsResultCount) {
      *codepointsResultCount = 0;
    }
    return nullptr;
  }

  int codepointsNoDupsCount = codepointCount;
  int *codepointsNoDups =
      static_cast<int *>(calloc(codepointCount, sizeof(int)));
  if (!codepointsNoDups) {
    log_error("Failed to allocate memory for codepoint deduplication");
    *codepointsResultCount = 0;
    return nullptr;
  }

  memcpy(codepointsNoDups, codepoints, codepointCount * sizeof(int));

  // Remove duplicates
  for (int i = 0; i < codepointsNoDupsCount; i++) {
    for (int j = i + 1; j < codepointsNoDupsCount; j++) {
      if (codepointsNoDups[i] == codepointsNoDups[j]) {
        for (int k = j; k < codepointsNoDupsCount - 1; k++) {
          codepointsNoDups[k] = codepointsNoDups[k + 1];
        }
        codepointsNoDupsCount--;
        j--;
      }
    }
  }

  // NOTE: The size of codepointsNoDups is the same as original array but
  // only required positions are filled (codepointsNoDupsCount)
  *codepointsResultCount = codepointsNoDupsCount;
  return codepointsNoDups;
}

// Convenience function to load a font with codepoints extracted from a string
// Automatically extracts codepoints, removes duplicates, and loads the font
// The caller is responsible for unloading the font when done
inline raylib::Font load_font_for_string(const std::string &content,
                                         const std::string &font_filename,
                                         int size = 96) {
  if (content.empty() || font_filename.empty()) {
    log_warn("Empty content or font filename passed to load_font_for_string");
    return raylib::GetFontDefault();
  }

  int codepointCount = 0;
  int *codepoints = raylib::LoadCodepoints(content.c_str(), &codepointCount);

  if (!codepoints || codepointCount <= 0) {
    log_warn("Failed to extract codepoints from string");
    return raylib::GetFontDefault();
  }

  int codepointNoDupsCounts = 0;
  int *codepointsNoDups = remove_duplicate_codepoints(
      codepoints, codepointCount, &codepointNoDupsCounts);

  raylib::UnloadCodepoints(codepoints);

  if (!codepointsNoDups || codepointNoDupsCounts <= 0) {
    log_warn("Failed to process codepoints for font loading");
    return raylib::GetFontDefault();
  }

  raylib::Font font = raylib::LoadFontEx(
      font_filename.c_str(), size, codepointsNoDups, codepointNoDupsCounts);
  raylib::SetTextureFilter(font.texture, raylib::TEXTURE_FILTER_BILINEAR);

  // Free the deduplicated codepoints array
  free(codepointsNoDups);

  return font;
}

inline float measure_text_internal(const char *content, const float size) {
  return static_cast<float>(
      raylib::MeasureText(content, static_cast<int>(size)));
}
inline raylib::Vector2 measure_text(const raylib::Font font,
                                    const char *content, const float size,
                                    const float spacing) {
  return raylib::MeasureTextEx(font, content, size, spacing);
}

// Add proper UTF-8 text measurement for CJK support
inline raylib::Vector2 measure_text_utf8(const raylib::Font font,
                                         const char *content, const float size,
                                         const float spacing) {
  if (!content) {
    log_warn("Null content passed to measure_text_utf8");
    return raylib::Vector2{0, 0};
  }

  if (size <= 0) {
    log_warn("Invalid font size {} passed to measure_text_utf8", size);
    return raylib::Vector2{0, 0};
  }

  // Use the existing measure_text for now, but this could be enhanced
  // to handle CJK text better with proper glyph spacing
  return raylib::MeasureTextEx(font, content, size, spacing);
}

// Get the left-side bearing (offsetX) for the first glyph in a string.
// Returns 0 if the string is empty or the glyph is not found in the font.
inline float get_first_glyph_bearing(const raylib::Font font, const char *text) {
  if (!text || text[0] == '\0') return 0.0f;
  int bytesProcessed = 0;
  int codepoint = raylib::GetCodepoint(text, &bytesProcessed);
  int glyphIndex = raylib::GetGlyphIndex(font, codepoint);
  if (glyphIndex < 0 || glyphIndex >= font.glyphCount) return 0.0f;
  return static_cast<float>(font.glyphs[glyphIndex].offsetX);
}
#elif defined(AFTER_HOURS_USE_METAL)
// ── Metal / fontstash path ──
// Font is an int (fontstash font ID).  We wrap it so the type is distinct.
struct Font {
  int id = FONS_INVALID;
};

inline Font load_font_from_file(const char *file) {
  auto* ctx = graphics::metal_detail::g_fons_ctx;
  if (!ctx) {
    log_warn("fontstash context not ready yet (load_font_from_file)");
    return Font{FONS_INVALID};
  }
  int id = fonsAddFont(ctx, file, file);
  if (id == FONS_INVALID) {
    log_warn("Failed to load font: {}", file);
  }
  return Font{id};
}

inline Font load_font_from_file_with_codepoints(const char *file, int *, int) {
  // fontstash loads full TTF; codepoint filtering not needed
  return load_font_from_file(file);
}

inline int *remove_duplicate_codepoints(int *, int,
                                        int *codepointsResultCount) {
  if (codepointsResultCount) *codepointsResultCount = 0;
  return nullptr;
}

inline Font load_font_for_string(const std::string &, const std::string &font_file,
                                 int = 96) {
  return load_font_from_file(font_file.c_str());
}

inline float measure_text_internal(const char *text, const float size) {
  auto* ctx = graphics::metal_detail::g_fons_ctx;
  if (!ctx || graphics::metal_detail::g_active_font == FONS_INVALID) return 0.f;
  fonsSetFont(ctx, graphics::metal_detail::g_active_font);
  fonsSetSize(ctx, size);
  return fonsTextBounds(ctx, 0, 0, text, nullptr, nullptr);
}

inline Vector2Type measure_text(const Font font, const char *text,
                                const float size, const float /*spacing*/) {
  auto* ctx = graphics::metal_detail::g_fons_ctx;
  if (!ctx) return Vector2Type{0, 0};
  int fid = (font.id != FONS_INVALID) ? font.id : graphics::metal_detail::g_active_font;
  if (fid == FONS_INVALID) return Vector2Type{0, 0};
  fonsSetFont(ctx, fid);
  fonsSetSize(ctx, size);
  float bounds[4] = {};
  fonsTextBounds(ctx, 0, 0, text, nullptr, bounds);
  float w = bounds[2] - bounds[0];
  float ascender, descender, lineh;
  fonsVertMetrics(ctx, &ascender, &descender, &lineh);
  return Vector2Type{w, lineh};
}

inline Vector2Type measure_text_utf8(const Font font, const char *text,
                                     const float size, const float spacing) {
  return measure_text(font, text, size, spacing);
}

inline float get_first_glyph_bearing(const Font, const char *) {
  return 0.0f;
}

#else
// ── Generic fallback (no backend) ──
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
#endif
} // namespace afterhours
