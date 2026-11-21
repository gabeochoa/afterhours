
#pragma once

#include <algorithm>
#include <map>
#include <optional>
#include <vector>

#include "../../font_coverage.h"
#include "../../graphics_common.h"
#include "../../measure_memo.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>

#include "../../developer.h"
#include "../../warn_once.h"

namespace afterhours {

using Font = raylib::Font;
using vec2 = raylib::Vector2;

namespace detail {
inline std::optional<raylib::Font> &default_font_override() {
  static std::optional<raylib::Font> font;
  return font;
}
} // namespace detail

// The face for text drawn without one: get_default_font(), get_unset_font(),
// draw_text and the library's debug overlays. Unset, it is raylib's built-in
// pixel font. Set it before the UI plugin loads its fonts so imm's default
// and unset fonts pick it up too. The caller keeps the font loaded.
inline void set_default_font(raylib::Font font) {
  detail::default_font_override() = font;
}
inline void clear_default_font() { detail::default_font_override().reset(); }

inline raylib::Font get_default_font() {
  return detail::default_font_override().value_or(raylib::GetFontDefault());
}
inline raylib::Font get_unset_font() { return get_default_font(); }

// Display pixels per logical pixel for text: the window's DPI scale when a
// window exists, otherwise the supersample factor the backend set at init.
inline float font_display_scale() {
  float scale = static_cast<float>(graphics::render_scale());
  if (raylib::IsWindowReady()) {
    const float dpi = raylib::GetWindowScaleDPI().x;
    if (dpi > scale)
      scale = dpi;
  }
  return scale > 1.f ? scale : 1.f;
}

// A nominal (logical) size scaled to display pixels, for callers that want
// glyphs rasterized at the density the display actually shows.
inline int display_scaled_size(int nominal) {
  const float scaled = std::round(static_cast<float>(nominal) *
                                  font_display_scale());
  return scaled >= 1.f ? static_cast<int>(scaled) : 1;
}

// Passing nullptr asks raylib for its default set, which is 95 ASCII glyphs.
// So an arrow, a chevron, a checkmark or an ellipsis drew nothing at all, and
// both floatinghotel and hanabi shipped invisible UI furniture before noticing.
// These are the ranges a UI actually types: Latin-1 for accents, punctuation
// for dashes and the ellipsis, then arrows, box drawing, shapes and check
// marks. A glyph the face does not have is still skipped by raylib, so this
// widens what CAN render rather than promising it will.
inline std::vector<int> default_codepoints() {
  static constexpr int kRanges[][2] = {
      {0x0020, 0x007E}, // ASCII
      {0x00A0, 0x00FF}, // Latin-1 supplement
      {0x2010, 0x2027}, // dashes, quotes, ellipsis
      {0x2190, 0x21FF}, // arrows
      {0x2500, 0x257F}, // box drawing
      {0x25A0, 0x25FF}, // geometric shapes
      {0x2713, 0x2717}, // check and cross
      {0xFFFD, 0xFFFD},
  };
  std::vector<int> cps;
  for (const auto &r : kRanges)
    for (int c = r[0]; c <= r[1]; c++)
      cps.push_back(c);
  return cps;
}

// LoadFontEx returns glyphs but no texture when there is no GL context, and a
// font with glyphs and texture.id 0 draws nothing. Build the atlas by hand
// instead of failing. Both wm_afterhours and kart-afterhours had written this
// same routine in their own preload before it lived here.
// Skyline (packMethod 1) rather than raylib's simple row packer, and padding
// wide enough to survive downscaling.
//
// The row packer drops glyphs on a large atlas -- raylib logs "Failed to
// package character" -- and the library's own default_codepoints() is 556, so
// the shipped default is the likeliest case to hit it. 1px of padding also
// bleeds between glyphs when an app downscales the atlas under mipmapping.
enum struct FontAtlasPacker {
  Row = 0,     // raylib's default; drops glyphs on a large atlas
  Skyline = 1, // slower to pack, fits more
};

// Auto is the historical choice (trilinear with mipmaps, bilinear without).
enum struct FontAtlasFilter {
  Auto = -1,
  Point = raylib::TEXTURE_FILTER_POINT,
  Bilinear = raylib::TEXTURE_FILTER_BILINEAR,
  Trilinear = raylib::TEXTURE_FILTER_TRILINEAR,
};

struct FontAtlasConfig {
  int padding = 4;
  FontAtlasPacker packer = FontAtlasPacker::Skyline;
  FontAtlasFilter filter = FontAtlasFilter::Auto;
};

inline raylib::TextureFilter
resolve_font_filter(const FontAtlasConfig &config, int mipmaps) {
  switch (config.filter) {
    case FontAtlasFilter::Point:
      return raylib::TEXTURE_FILTER_POINT;
    case FontAtlasFilter::Bilinear:
      return raylib::TEXTURE_FILTER_BILINEAR;
    case FontAtlasFilter::Trilinear:
      return raylib::TEXTURE_FILTER_TRILINEAR;
    case FontAtlasFilter::Auto:
      break;
  }
  return mipmaps > 1 ? raylib::TEXTURE_FILTER_TRILINEAR
                     : raylib::TEXTURE_FILTER_BILINEAR;
}

// What every load uses unless a caller passes its own. Set it once at startup:
// fonts are loaded from several places, and threading a parameter through all
// of them would move the problem rather than solve it.
//
//   set_font_atlas_config({.padding = 8, .packer = FontAtlasPacker::Row});
//
// Unnamed fields keep their defaults, so setting one does not silently reset
// the others.
inline FontAtlasConfig &default_font_atlas_config() {
  static FontAtlasConfig config;
  return config;
}

inline void set_font_atlas_config(const FontAtlasConfig &config) {
  default_font_atlas_config() = config;
}

// True cmap coverage per loaded font, keyed the way measure_text keys a
// font. Recorded at load: afterwards there is no way to tell a real glyph
// from the placeholder raylib inserts for a requested-but-absent codepoint.
inline std::map<uint64_t, font_coverage::Ranges> &coverage_registry() {
  static std::map<uint64_t, font_coverage::Ranges> m;
  return m;
}

inline uint64_t font_key(const raylib::Font font) {
  return (static_cast<uint64_t>(font.texture.id) << 32) ^
         (static_cast<uint64_t>(font.baseSize) << 16) ^
         static_cast<uint64_t>(font.glyphCount);
}

// (font, text) warn memo; generation bumps on coverage change/unload.
inline uint64_t &glyph_warn_generation() {
  static uint64_t g = 0;
  return g;
}
inline std::map<std::pair<uint64_t, std::string>, uint64_t> &
glyph_warn_cache() {
  static std::map<std::pair<uint64_t, std::string>, uint64_t> m;
  return m;
}

inline std::vector<raylib::Font> &fallback_faces() {
  static std::vector<raylib::Font> faces;
  return faces;
}

inline void record_font_coverage(const raylib::Font font, const char *file) {
  if (font.glyphCount <= 0)
    return;
  font_coverage::Ranges ranges;
  if (!font_coverage::parse_file(file, ranges))
    return;
  coverage_registry()[font_key(font)] = std::move(ranges);
  glyph_warn_generation()++;
  auto &faces = fallback_faces();
  if (std::none_of(faces.begin(), faces.end(), [&](const raylib::Font &f) {
        return f.glyphs == font.glyphs;
      }))
    faces.push_back(font);
}

// Fonts rasterize large (96px) and are usually drawn far smaller. With
// plain bilinear filtering and no mipmaps, a 10x downscale samples four
// texels per pixel and thin strokes fall between them: glyphs fragment
// and fade at sub-1.0 UI scale. Mipmaps + trilinear average the atlas
// properly at every size.
inline void prepare_font_texture(
    raylib::Font &font,
    const FontAtlasConfig &atlas_config = default_font_atlas_config()) {
  if (font.texture.id == 0)
    return;
  raylib::GenTextureMipmaps(&font.texture);
  raylib::SetTextureFilter(
      font.texture,
      resolve_font_filter(atlas_config, font.texture.mipmaps));
}

inline raylib::Font
build_font_atlas(const char *file, int px, const int *codepoints, int count,
                 const FontAtlasConfig &atlas_config =
                     default_font_atlas_config()) {
  raylib::Font font{};
  int data_size = 0;
  unsigned char *data = raylib::LoadFileData(file, &data_size);
  if (!data || data_size <= 0) {
    log_error("build_font_atlas: could not read '{}'", file);
    return font;
  }
  font.baseSize = px;
  font.glyphCount = count;
  font.glyphPadding = atlas_config.padding;
  font.glyphs = raylib::LoadFontData(data, data_size, px,
                                     const_cast<int *>(codepoints), count,
                                     raylib::FONT_DEFAULT);
  if (!font.glyphs) {
    log_error("build_font_atlas: no glyph data from '{}'", file);
    raylib::UnloadFileData(data);
    return raylib::Font{};
  }
  raylib::Image atlas = raylib::GenImageFontAtlas(
      font.glyphs, &font.recs, font.glyphCount, px, atlas_config.padding,
      static_cast<int>(atlas_config.packer));
  font.texture = raylib::LoadTextureFromImage(atlas);
  raylib::UnloadImage(atlas);
  raylib::UnloadFileData(data);
  record_font_coverage(font, file);
  return font;
}

inline bool font_gl_ready() {
  return raylib::IsWindowReady() || graphics::is_headless();
}

inline raylib::Font load_font_from_file(const char *file, int size = 0) {
  std::vector<int> cps = default_codepoints();
  // The default size is nominal 32px, rasterized at display pixels; an
  // explicit size stays exactly what the caller asked for.
  const int px = size > 0 ? size : display_scaled_size(32);
  raylib::Font font =
      raylib::LoadFontEx(file, px, cps.data(), (int)cps.size());
  if (font.glyphCount <= 0) {
    // Fall back rather than leaving the caller with no font at all.
    font = (size > 0) ? raylib::LoadFontEx(file, size, nullptr, 0)
                      : raylib::LoadFont(file);
  }
  // Glyphs but no texture is the headless case, and it draws nothing.
  if (font.texture.id == 0 && font_gl_ready())
    font = build_font_atlas(file, px, cps.data(), (int)cps.size());
  prepare_font_texture(font);
  record_font_coverage(font, file);
  return font;
}

// Rasterize at the display's pixel size for a nominal logical size; for
// callers passing an explicit size that they want scaled, unlike the loaders
// above, whose explicit sizes are honored verbatim.
inline raylib::Font load_font_at_display_size(const char *file, int nominal = 32) {
  return load_font_from_file(file, display_scaled_size(nominal));
}

inline raylib::Font load_font_from_memory(const unsigned char *data, int size,
                                          const char *name, int px = 0) {
  if (px <= 0)
    px = display_scaled_size(32);
  std::vector<int> cps = default_codepoints();
  raylib::Font font = raylib::LoadFontFromMemory(
      ".ttf", data, size, px, cps.data(), static_cast<int>(cps.size()));
  if (font.glyphCount <= 0) {
    log_warn("Failed to load font from memory: {}", name ? name : "");
    return font;
  }
  prepare_font_texture(font);
  font_coverage::Ranges ranges;
  if (font_coverage::parse_cmap(data, static_cast<size_t>(size), ranges)) {
    coverage_registry()[font_key(font)] = std::move(ranges);
    glyph_warn_generation()++;
  }
  return font;
}

// Whether the loaded face actually covers a codepoint. raylib maps anything it
// does not have to glyph 0, so asking is the only way to tell a real glyph
// from a silent nothing.
inline bool font_has_glyph(const raylib::Font font, int codepoint) {
  if (codepoint < 0 || font.glyphCount <= 0 || !font.glyphs)
    return false;
  const int idx = raylib::GetGlyphIndex(font, codepoint);
  if (idx < 0 || idx >= font.glyphCount)
    return false;
  // GetGlyphIndex falls back to the index of '?', not to 0, so a nonzero index
  // says nothing on its own -- it is the answer for every codepoint the face
  // lacks. Only the entry actually holding this codepoint counts.
  if (font.glyphs[idx].value != codepoint)
    return false;
  // That entry is not proof either: LoadFontData inserts one for every
  // requested codepoint, pointing absent ones at glyph 0's blank. When the
  // face's cmap was recorded at load, it is the authority.
  const auto it = coverage_registry().find(font_key(font));
  if (it != coverage_registry().end())
    return font_coverage::covers(it->second,
                                 static_cast<uint32_t>(codepoint));
  return true;
}

inline raylib::Font face_for(const raylib::Font font, int codepoint) {
  if (codepoint < 0x80 || font_has_glyph(font, codepoint))
    return font;
  for (const raylib::Font &face : fallback_faces())
    if (font_has_glyph(face, codepoint))
      return face;
  return font;
}

inline const char *drawable_text(const raylib::Font font, const char *text,
                                 std::string &storage) {
  const auto covered = [&](uint32_t cp) {
    const int c = static_cast<int>(cp);
    return font_has_glyph(face_for(font, c), c);
  };
  return font_coverage::visible_text(text, covered, storage) ? storage.c_str()
                                                             : text;
}

struct FaceRun {
  raylib::Font face;
  std::string text;
};

inline std::vector<FaceRun> fallback_runs(const raylib::Font font,
                                          const char *text) {
  std::vector<FaceRun> runs;
  if (!text || std::strchr(text, '\n'))
    return runs;
  bool fallback = false;
  for (size_t i = 0; text[i];) {
    int advance = 0;
    const int cp = raylib::GetCodepointNext(text + i, &advance);
    const size_t len = advance > 0 ? static_cast<size_t>(advance) : 1;
    const raylib::Font face = face_for(font, cp);
    fallback = fallback || face.glyphs != font.glyphs;
    if (runs.empty() || runs.back().face.glyphs != face.glyphs)
      runs.push_back({face, {}});
    runs.back().text.append(text + i, len);
    i += len;
  }
  if (!fallback)
    runs.clear();
  return runs;
}

// A codepoint the font lacks draws nothing: no box, no warning, no log. Both
// floatinghotel and hanabi shipped UI with invisible arrows and chevrons and
// found out by looking. load_font_from_file asks raylib for the default set,
// which is 95 ASCII glyphs, so any symbol is a blank unless the caller loaded
// codepoints for it.

// Test counter: decode-loop runs.
inline size_t &glyph_warn_decode_runs() {
  static size_t n = 0;
  return n;
}

inline void warn_on_missing_glyphs(const raylib::Font font, const char *text) {
  if (!text)
    return;
  // Decode and ask the font, rather than warning on any byte >= 0x80. The
  // byte-range guess fired on every correctly-rendered CJK string -- an app
  // that loaded the right codepoints got warned for text it draws perfectly,
  // which is how the warning ended up being ignored, taking the real missing
  // glyphs with it.
  // Pure ASCII cannot be missing -- the default set is the 95 ASCII glyphs --
  // so answer it with a byte scan instead of a decode per character.
  bool ascii_only = true;
  for (int i = 0; text[i] != '\0'; i++)
    if (static_cast<unsigned char>(text[i]) >= 0x80) {
      ascii_only = false;
      break;
    }
  if (ascii_only)
    return;

  auto &cache = glyph_warn_cache();
  const auto key = std::make_pair(font_key(font), std::string(text));
  const auto cached = cache.find(key);
  if (cached != cache.end() && cached->second == glyph_warn_generation())
    return;

  glyph_warn_decode_runs()++;
  for (int i = 0; text[i] != '\0';) {
    int advance = 0;
    const int cp = raylib::GetCodepointNext(&text[i], &advance);
    i += advance > 0 ? advance : 1;
    if (cp < 0x80)
      continue; // the default set is the 95 ASCII glyphs
    if (font_has_glyph(font, cp))
      continue;
    warn_once(0x110000 + cp,
              "font has no glyph for U+{:04X}, which draws as a replacement "
              "glyph. Load the font with codepoints for the ranges you use.",
              cp);
  }
  if (cache.size() > 4096)
    cache.clear(); // cap growth
  cache[key] = glyph_warn_generation();
}

// Add codepoint-based font loading for CJK support
inline raylib::Font
load_font_from_file_with_codepoints(const char *file, int *codepoints,
                                    int codepoint_count, int size = 0) {
  if (size <= 0)
    size = display_scaled_size(32);
  if (!file || !codepoints || codepoint_count <= 0) {
    log_error("Invalid parameters for font loading: file={}, codepoints={}, "
              "count={}",
              file ? file : "null", codepoints ? "valid" : "null",
              codepoint_count);
    return get_default_font();
  }
  raylib::Font font = raylib::LoadFontEx(file, size, codepoints, codepoint_count);
  // See load_font_from_file: no GL context means no texture.
  if (font.texture.id == 0 && font_gl_ready())
    font = build_font_atlas(file, size, codepoints, codepoint_count);
  prepare_font_texture(font);
  record_font_coverage(font, file);
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
inline void unload_font(raylib::Font &font) {
  coverage_registry().erase(font_key(font));
  glyph_warn_generation()++;
  measure_memo::invalidate();
  raylib::UnloadFont(font);
  font = raylib::Font{};
}

inline raylib::Font load_font_for_string(const std::string &content,
                                         const std::string &font_filename,
                                         int size = 0) {
  if (size <= 0)
    size = display_scaled_size(96);
  if (content.empty() || font_filename.empty()) {
    log_warn("Empty content or font filename passed to load_font_for_string");
    return get_default_font();
  }

  int codepointCount = 0;
  int *codepoints = raylib::LoadCodepoints(content.c_str(), &codepointCount);

  if (!codepoints || codepointCount <= 0) {
    log_warn("Failed to extract codepoints from string");
    return get_default_font();
  }

  int codepointNoDupsCounts = 0;
  int *codepointsNoDups = remove_duplicate_codepoints(
      codepoints, codepointCount, &codepointNoDupsCounts);

  raylib::UnloadCodepoints(codepoints);

  if (!codepointsNoDups || codepointNoDupsCounts <= 0) {
    log_warn("Failed to process codepoints for font loading");
    return get_default_font();
  }

  raylib::Font font = raylib::LoadFontEx(
      font_filename.c_str(), size, codepointsNoDups, codepointNoDupsCounts);
  // See load_font_from_file: no GL context means no texture, and an
  // atlas-less font measures by estimate and draws nothing. The atlas
  // rebuild uploads a texture, which needs a window: windowless it
  // calls through a null GL function pointer and crashes, so there
  // the LoadFontEx result (glyphs, no texture) stands.
  if (font.texture.id == 0 && font_gl_ready())
    font = build_font_atlas(font_filename.c_str(), size, codepointsNoDups,
                            codepointNoDupsCounts);
  prepare_font_texture(font);
  record_font_coverage(font, font_filename.c_str());

  // Free the deduplicated codepoints array
  free(codepointsNoDups);

  return font;
}

// raylib builds its default font during InitWindow, so headless runs get one
// with no atlas. MeasureTextEx returns {0,0} for it rather than crashing,
// which silently collapses every text-sized widget to nothing.
inline bool font_is_usable(const raylib::Font font) {
  return font.texture.id != 0 && font.glyphCount > 0 && font.recs != nullptr;
}

// Rough advance-per-pixel-of-height for proportional Latin text. Only used
// when there is no atlas to measure against; tune if your headless layout
// assertions sit close to a wrap boundary.
inline constexpr float kUnmeasurableGlyphAspect = 0.5f;

inline raylib::Vector2 estimate_text_size(const char *content,
                                          const float size) {
  const size_t len = content ? std::strlen(content) : 0;
  return raylib::Vector2{static_cast<float>(len) * size *
                             kUnmeasurableGlyphAspect,
                         size};
}

inline raylib::Vector2 measure_text(const raylib::Font font,
                                    const char *content, const float size,
                                    const float spacing) {
  if (!std::isfinite(size) || size <= 0.f || !std::isfinite(spacing))
    return raylib::Vector2{0, 0};
  if (!font_is_usable(font)) {
    warn_once(0, "no glyph atlas to measure against, so text is being "
                 "estimated at {}x its height per character. Expected "
                 "headless -- load a font to measure for real.",
              kUnmeasurableGlyphAspect);
    return estimate_text_size(content, size);
  }
  // baseSize and glyphCount alongside the texture id, so a handle reused by a
  // differently built atlas is a different key rather than a stale hit.
  const std::uint64_t font_id = font_key(font);
  const auto key =
      measure_memo::key_for(content ? content : "", size, spacing, font_id);
  Vector2Type cached{};
  if (measure_memo::lookup(key, cached))
    return cached;
  std::string shown;
  const char *visible = drawable_text(font, content, shown);
  const std::vector<FaceRun> runs = fallback_runs(font, visible);
  raylib::Vector2 measured{0.f, 0.f};
  if (runs.empty())
    measured = raylib::MeasureTextEx(font, visible, size, spacing);
  for (size_t i = 0; i < runs.size(); i++) {
    const raylib::Vector2 run = raylib::MeasureTextEx(
        runs[i].face, runs[i].text.c_str(), size, spacing);
    measured.x += run.x + (i > 0 ? spacing : 0.f);
    measured.y = std::max(measured.y, run.y);
  }
  measure_memo::store(key, measured);
  return measured;
}

// Add proper UTF-8 text measurement for CJK support
inline raylib::Vector2 measure_text_utf8(const raylib::Font font,
                                         const char *content, const float size,
                                         const float spacing) {
  if (!content) {
    log_warn("Null content passed to measure_text_utf8");
    return raylib::Vector2{0, 0};
  }

  if (!std::isfinite(size) || size <= 0) {
    log_warn("Invalid font size {} passed to measure_text_utf8", size);
    return raylib::Vector2{0, 0};
  }

  return measure_text(font, content, size, spacing);
}

inline float monospace_cell_advance(const raylib::Font font,
                                    const float size) {
  if (!std::isfinite(size) || size <= 0.f)
    return 0.f;
  if (!font_is_usable(font)) {
    warn_once(0, "no glyph atlas to measure against, so text is being "
                 "estimated at {}x its height per character. Expected "
                 "headless -- load a font to measure for real.",
              kUnmeasurableGlyphAspect);
    return size * kUnmeasurableGlyphAspect;
  }
  if (font.baseSize <= 0 || !font.glyphs || !font.recs)
    return 0.f;
  const int idx = raylib::GetGlyphIndex(font, '0');
  if (idx < 0 || idx >= font.glyphCount || font.glyphs[idx].value != '0')
    return 0.f;
  const float advance =
      font.glyphs[idx].advanceX == 0
          ? static_cast<float>(font.recs[idx].width)
          : static_cast<float>(font.glyphs[idx].advanceX);
  return advance * size / static_cast<float>(font.baseSize);
}

// Get the left-side bearing (offsetX) for the first glyph in a string.
inline float get_first_glyph_bearing(const raylib::Font font,
                                     const char *text) {
  if (!text || text[0] == '\0')
    return 0.0f;
  int bytesProcessed = 0;
  int codepoint = raylib::GetCodepoint(text, &bytesProcessed);
  int glyphIndex = raylib::GetGlyphIndex(font, codepoint);
  if (glyphIndex < 0 || glyphIndex >= font.glyphCount)
    return 0.0f;
  return static_cast<float>(font.glyphs[glyphIndex].offsetX);
}

inline bool is_font_loaded(const raylib::Font &font) {
  return font.texture.id != 0 && font.glyphCount > 0;
}

} // namespace afterhours
