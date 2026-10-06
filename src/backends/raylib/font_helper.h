
#pragma once

#include <algorithm>
#include <map>
#include <vector>

#include "../../font_coverage.h"
#include "../../measure_memo.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include "../../developer.h"
#include "../../warn_once.h"

namespace afterhours {

using Font = raylib::Font;
using vec2 = raylib::Vector2;

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

struct FontAtlasConfig {
  int padding = 4;
  FontAtlasPacker packer = FontAtlasPacker::Skyline;
};

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

// hhea baseline ratio per loaded font (font_coverage::baseline_ratio).
inline std::map<uint64_t, float> &baseline_registry() {
  static std::map<uint64_t, float> m;
  return m;
}

inline void record_font_data(const raylib::Font font, const unsigned char *d,
                             size_t n) {
  if (font.glyphCount <= 0)
    return;
  font_coverage::Ranges ranges;
  if (font_coverage::parse_cmap(d, n, ranges))
    coverage_registry()[font_key(font)] = std::move(ranges);
  if (const float r = font_coverage::baseline_ratio(d, n); r > 0.f)
    baseline_registry()[font_key(font)] = r;
}

inline void record_font_coverage(const raylib::Font font, const char *file) {
  if (font.glyphCount <= 0)
    return;
  std::vector<unsigned char> bytes;
  if (font_coverage::read_file(file, bytes))
    record_font_data(font, bytes.data(), bytes.size());
}

// Fonts rasterize large (96px) and are usually drawn far smaller. With
// plain bilinear filtering and no mipmaps, a 10x downscale samples four
// texels per pixel and thin strokes fall between them: glyphs fragment
// and fade at sub-1.0 UI scale. Mipmaps + trilinear average the atlas
// properly at every size.
inline void prepare_font_texture(raylib::Font &font) {
  if (font.texture.id == 0)
    return;
  raylib::GenTextureMipmaps(&font.texture);
  raylib::SetTextureFilter(font.texture, font.texture.mipmaps > 1
                                             ? raylib::TEXTURE_FILTER_TRILINEAR
                                             : raylib::TEXTURE_FILTER_BILINEAR);
}

inline raylib::Font
build_font_atlas_from_memory(const unsigned char *data, int data_size, int px,
                             const int *codepoints, int count,
                             const FontAtlasConfig &atlas_config =
                                 default_font_atlas_config()) {
  raylib::Font font{};
  font.baseSize = px;
  font.glyphCount = count;
  font.glyphPadding = atlas_config.padding;
  font.glyphs = raylib::LoadFontData(data, data_size, px,
                                     const_cast<int *>(codepoints), count,
                                     raylib::FONT_DEFAULT);
  if (!font.glyphs) {
    log_error("build_font_atlas: no glyph data");
    return raylib::Font{};
  }
  raylib::Image atlas = raylib::GenImageFontAtlas(
      font.glyphs, &font.recs, font.glyphCount, px, atlas_config.padding,
      static_cast<int>(atlas_config.packer));
  font.texture = raylib::LoadTextureFromImage(atlas);
  raylib::UnloadImage(atlas);
  record_font_data(font, data, static_cast<size_t>(data_size));
  return font;
}

inline raylib::Font
build_font_atlas(const char *file, int px, const int *codepoints, int count,
                 const FontAtlasConfig &atlas_config =
                     default_font_atlas_config()) {
  int data_size = 0;
  unsigned char *data = raylib::LoadFileData(file, &data_size);
  if (!data || data_size <= 0) {
    log_error("build_font_atlas: could not read '{}'", file);
    return raylib::Font{};
  }
  raylib::Font font = build_font_atlas_from_memory(data, data_size, px,
                                                   codepoints, count,
                                                   atlas_config);
  raylib::UnloadFileData(data);
  return font;
}

inline raylib::Font load_font_from_file(const char *file, int size = 0) {
  std::vector<int> cps = default_codepoints();
  const int px = size > 0 ? size : 32;
  raylib::Font font =
      raylib::LoadFontEx(file, px, cps.data(), (int)cps.size());
  if (font.glyphCount <= 0) {
    // Fall back rather than leaving the caller with no font at all.
    font = (size > 0) ? raylib::LoadFontEx(file, size, nullptr, 0)
                      : raylib::LoadFont(file);
  }
  // Glyphs but no texture is the headless case, and it draws nothing.
  if (font.texture.id == 0)
    font = build_font_atlas(file, px, cps.data(), (int)cps.size());
  prepare_font_texture(font);
  record_font_coverage(font, file);
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

// One face of a font file, by index, so a collection (.ttc) loads: most
// system CJK faces ship as collections and raylib reads only a plain font.
// raylib bakes glyphs at load, so `codepoints` is what the face can ever
// draw -- a CJK fallback needs the ideographs it is for (empty means
// default_codepoints(), which has none).
inline raylib::Font load_font_face(const char *file, int face_index,
                                   int size = 0,
                                   const std::vector<int> &codepoints = {}) {
  std::vector<unsigned char> bytes;
  if (!font_coverage::read_file(file, bytes)) {
    log_warn("Failed to read font: {}", file ? file : "(null)");
    return raylib::Font{};
  }
  std::vector<unsigned char> face =
      font_coverage::extract_face(bytes, face_index);
  bytes = {};
  if (face.empty()) {
    log_warn("No face {} in font: {}", face_index, file);
    return raylib::Font{};
  }
  std::vector<int> cps = codepoints.empty() ? default_codepoints() : codepoints;
  const int px = size > 0 ? size : 32;
  raylib::Font font = raylib::LoadFontFromMemory(
      ".ttf", face.data(), static_cast<int>(face.size()), px, cps.data(),
      static_cast<int>(cps.size()));
  if (font.texture.id == 0)
    font = build_font_atlas_from_memory(face.data(),
                                        static_cast<int>(face.size()), px,
                                        cps.data(), static_cast<int>(cps.size()));
  prepare_font_texture(font);
  record_font_data(font, face.data(), face.size());
  return font;
}

// Fallback chain per primary, keyed like measure_text keys a font.
inline std::map<uint64_t, std::vector<raylib::Font>> &fallback_registry() {
  static std::map<uint64_t, std::vector<raylib::Font>> m;
  return m;
}

// Glyphs `primary` lacks are measured and drawn from the first face in
// `chain` that has them; the chain replaces any earlier one. raylib has no
// fallback of its own, so measure_text and draw_text_ex share place_glyphs
// below whenever a string needs it.
inline bool set_font_fallbacks(const raylib::Font primary,
                               const std::vector<raylib::Font> &chain) {
  if (primary.glyphCount <= 0)
    return false;
  std::vector<raylib::Font> kept;
  bool ok = true;
  for (const raylib::Font &f : chain) {
    if (f.glyphCount <= 0 || font_key(f) == font_key(primary)) {
      ok = false;
      continue;
    }
    kept.push_back(f);
  }
  fallback_registry()[font_key(primary)] = std::move(kept);
  measure_memo::invalidate();
  return ok;
}

inline std::vector<raylib::Font> font_fallbacks(const raylib::Font primary) {
  const auto it = fallback_registry().find(font_key(primary));
  return it == fallback_registry().end() ? std::vector<raylib::Font>{}
                                         : it->second;
}

inline float baseline_ratio_of(const raylib::Font font) {
  const auto it = baseline_registry().find(font_key(font));
  return it == baseline_registry().end() ? 0.8f : it->second;
}

// Distance from the top of a drawn line to its baseline, in pixels.
inline float font_baseline(const raylib::Font font, const float size) {
  return size * baseline_ratio_of(font);
}

// One codepoint as place_glyphs lays it out: which face, and where its pen
// sits relative to the text's top left.
struct PlacedGlyph {
  raylib::Font font;
  int codepoint = 0;
  float x = 0.f;
  float y = 0.f;
};

// The single per-codepoint layout measure_text and draw_text_ex share once a
// string leaves the primary face: a codepoint goes to the first face in
// [primary, chain...] that has it, anything none has becomes a replacement
// (font_coverage::visible_text), and fallback glyphs are moved onto the
// primary's baseline -- what fontstash does on the sokol backend. Advance
// and spacing follow DrawTextEx exactly.
//
// Returns false, with `out` meaningless, when every codepoint is the
// primary's own: then raylib's MeasureTextEx/DrawTextEx are already exact
// and stay in charge, so text that never needed a fallback is unchanged.
// '\n' moves down by size + 2, raylib's default line spacing.
inline bool place_glyphs(const raylib::Font primary, const char *text,
                         float size, float spacing,
                         std::vector<PlacedGlyph> &out, Vector2Type &extent) {
  out.clear();
  if (!text || primary.glyphCount <= 0)
    return false;
  bool ascii = true;
  for (int i = 0; text[i]; i++)
    if (static_cast<unsigned char>(text[i]) >= 0x80) {
      ascii = false;
      break;
    }
  if (ascii)
    return false;
  const std::vector<raylib::Font> chain = font_fallbacks(primary);
  const auto face_for = [&](int cp) -> const raylib::Font * {
    if (font_has_glyph(primary, cp))
      return &primary;
    for (const raylib::Font &f : chain)
      if (font_has_glyph(f, cp))
        return &f;
    return nullptr;
  };
  std::string visible;
  bool needed = font_coverage::visible_text(
      text, [&](uint32_t cp) { return face_for(static_cast<int>(cp)) != nullptr; },
      visible);
  const char *s = needed ? visible.c_str() : text;
  const float primary_base = size * baseline_ratio_of(primary);
  float x = 0.f, y = 0.f, width = 0.f;
  bool line_has_glyph = false;
  for (int i = 0; s[i];) {
    int bytes = 0;
    const int cp = raylib::GetCodepointNext(&s[i], &bytes);
    i += bytes > 0 ? bytes : 1;
    if (cp == '\n') {
      if (line_has_glyph)
        width = std::max(width, x - spacing);
      x = 0.f;
      y += size + 2.f;
      line_has_glyph = false;
      continue;
    }
    const raylib::Font *f = face_for(cp);
    if (!f)
      f = &primary; // e.g. '?' in a face without it: what raylib would do
    if (f != &primary)
      needed = true;
    const int gi = raylib::GetGlyphIndex(*f, cp);
    const float scale = size / static_cast<float>(f->baseSize);
    const float advance = f->glyphs[gi].advanceX > 0
                              ? static_cast<float>(f->glyphs[gi].advanceX)
                              : f->recs[gi].width +
                                    static_cast<float>(f->glyphs[gi].offsetX);
    const float dy =
        f == &primary ? 0.f : primary_base - size * baseline_ratio_of(*f);
    out.push_back(PlacedGlyph{*f, cp, x, y + dy});
    x += advance * scale + spacing;
    line_has_glyph = true;
  }
  if (line_has_glyph)
    width = std::max(width, x - spacing);
  extent = Vector2Type{width, y + size};
  return needed;
}

// A codepoint the font lacks draws nothing: no box, no warning, no log. Both
// floatinghotel and hanabi shipped UI with invisible arrows and chevrons and
// found out by looking. load_font_from_file asks raylib for the default set,
// which is 95 ASCII glyphs, so any symbol is a blank unless the caller loaded
// codepoints for it.
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

  for (int i = 0; text[i] != '\0';) {
    int advance = 0;
    const int cp = raylib::GetCodepointNext(&text[i], &advance);
    i += advance > 0 ? advance : 1;
    if (cp < 0x80)
      continue; // the default set is the 95 ASCII glyphs
    if (font_has_glyph(font, cp))
      continue;
    bool in_chain = false;
    for (const raylib::Font &f : font_fallbacks(font))
      in_chain = in_chain || font_has_glyph(f, cp);
    if (in_chain || font_coverage::is_invisible_modifier(cp))
      continue;
    warn_once(0x110000 + cp,
              "font has no glyph for U+{:04X}; it draws as a replacement "
              "glyph. Load the font with codepoints for the ranges you use, "
              "or give it a fallback face (set_font_fallbacks).",
              cp);
  }
}

// Add codepoint-based font loading for CJK support
inline raylib::Font
load_font_from_file_with_codepoints(const char *file, int *codepoints,
                                    int codepoint_count, int size = 32) {
  if (!file || !codepoints || codepoint_count <= 0) {
    log_error("Invalid parameters for font loading: file={}, codepoints={}, "
              "count={}",
              file ? file : "null", codepoints ? "valid" : "null",
              codepoint_count);
    return raylib::GetFontDefault();
  }
  raylib::Font font = raylib::LoadFontEx(file, size, codepoints, codepoint_count);
  // See load_font_from_file: no GL context means no texture.
  if (font.texture.id == 0)
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
  const std::uint64_t key =
      measure_memo::hash(content ? content : "", size, spacing, font_id);
  Vector2Type cached{};
  if (measure_memo::lookup(key, cached))
    return cached;
  std::vector<PlacedGlyph> glyphs;
  Vector2Type extent{};
  const raylib::Vector2 measured =
      place_glyphs(font, content, size, spacing, glyphs, extent)
          ? raylib::Vector2{extent.x, extent.y}
          : raylib::MeasureTextEx(font, content, size, spacing);
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

  if (size <= 0) {
    log_warn("Invalid font size {} passed to measure_text_utf8", size);
    return raylib::Vector2{0, 0};
  }

  return measure_text(font, content, size, spacing);
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
