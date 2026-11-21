
#pragma once

#include <algorithm>
#include <map>
#include <optional>
#include <set>
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
  auto &faces = fallback_faces();
  if (std::none_of(faces.begin(), faces.end(), [&](const raylib::Font &f) {
        return f.glyphs == font.glyphs;
      }))
    faces.push_back(font);
}

// Codepoints that rasterized but never made it into the atlas, keyed the
// way coverage is keyed. Indexed by font_key like coverage_registry().
inline std::map<uint64_t, std::vector<int>> &packing_failure_registry() {
  static std::map<uint64_t, std::vector<int>> failures;
  return failures;
}

inline bool glyph_rasterized(const raylib::GlyphInfo &glyph) {
  return glyph.image.width > 0 && glyph.image.height > 0 &&
         glyph.image.data != nullptr;
}

// Glyphs with an image but no atlas rect. A codepoint the face lacks has no
// image, so it never shows up here: this is packing, not coverage.
inline std::vector<int> glyph_packing_failures(const raylib::Font font) {
  std::vector<int> failed;
  if (!font.glyphs || !font.recs || font.glyphCount <= 0)
    return failed;
  for (int i = 0; i < font.glyphCount; i++) {
    const bool no_rect =
        font.recs[i].width <= 0.f || font.recs[i].height <= 0.f;
    if (glyph_rasterized(font.glyphs[i]) && no_rect)
      failed.push_back(font.glyphs[i].value);
  }
  const auto it = packing_failure_registry().find(font_key(font));
  if (it != packing_failure_registry().end())
    for (int cp : it->second)
      if (std::find(failed.begin(), failed.end(), cp) == failed.end())
        failed.push_back(cp);
  return failed;
}

// Skyline failures hide at the atlas corner: every unpacked glyph gets the
// same rec as the glyph packed there and its pixels are never copied, so
// only comparing atlas pixels against the glyph's own image tells it apart.
inline std::vector<int> find_unpacked_glyphs(
    const raylib::GlyphInfo *glyphs, const raylib::Rectangle *recs,
    int glyph_count, const raylib::Image &atlas, int padding) {
  std::vector<int> failed;
  if (!glyphs || !recs || atlas.data == nullptr || atlas.width <= 0 ||
      atlas.height <= 0)
    return failed;
  const auto *pixels = static_cast<const unsigned char *>(atlas.data);
  for (int i = 0; i < glyph_count; i++) {
    if (!glyph_rasterized(glyphs[i]))
      continue;
    // Only corner recs can be unpacked; everything else was packed.
    if (recs[i].x != static_cast<float>(padding) ||
        recs[i].y != static_cast<float>(padding))
      continue;
    const int x0 = static_cast<int>(recs[i].x);
    const int y0 = static_cast<int>(recs[i].y);
    const auto *image =
        static_cast<const unsigned char *>(glyphs[i].image.data);
    bool matches = true;
    for (int y = 0; y < glyphs[i].image.height && matches; y++)
      for (int x = 0; x < glyphs[i].image.width; x++) {
        const int dx = x0 + x, dy = y0 + y;
        if (dx < 0 || dy < 0 || dx >= atlas.width || dy >= atlas.height ||
            pixels[(dy * atlas.width + dx) * 2 + 1] !=
                image[y * glyphs[i].image.width + x]) {
          matches = false;
          break;
        }
      }
    if (!matches)
      failed.push_back(glyphs[i].value);
  }
  return failed;
}

// Record unpacked glyphs for FontManager::packing_failures and say it once
// per glyph: a full atlas otherwise draws the corner glyph for every victim
// with only a raylib TraceLog warning as evidence.
inline void report_glyph_packing_failures(const raylib::Font font,
                                          const char *what,
                                          std::vector<int> failed) {
  if (failed.empty())
    return;
  auto &stored = packing_failure_registry()[font_key(font)];
  static std::set<std::pair<uint64_t, int>> reported;
  for (int cp : failed) {
    if (std::find(stored.begin(), stored.end(), cp) != stored.end())
      continue;
    stored.push_back(cp);
    if (reported.insert({font_key(font), cp}).second)
      log_error("font atlas did not pack U+{:04X} from '{}': it draws the "
                "atlas-corner glyph instead. Use a smaller font size or "
                "fewer codepoints.",
                cp, what ? what : "");
  }
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
  const std::vector<int> unpacked = find_unpacked_glyphs(
      font.glyphs, font.recs, font.glyphCount, atlas, atlas_config.padding);
  font.texture = raylib::LoadTextureFromImage(atlas);
  raylib::UnloadImage(atlas);
  raylib::UnloadFileData(data);
  record_font_coverage(font, file);
  report_glyph_packing_failures(font, file, unpacked);
  return font;
}

inline bool font_gl_ready() {
  return raylib::IsWindowReady() || graphics::is_headless();
}

// Report what a finished load left behind: unpacked glyphs, or glyphs with
// no atlas texture at all (an atlas too large to upload draws nothing and
// measures by estimate, with only a lazy measure-time warning otherwise).
inline void check_font_atlas(const raylib::Font font, const char *what) {
  report_glyph_packing_failures(font, what, glyph_packing_failures(font));
  if (font.glyphCount <= 0 || font.texture.id != 0 || !font_gl_ready())
    return;
  static std::set<std::pair<uint64_t, std::string>> reported;
  if (reported.insert({font_key(font), what ? what : ""}).second)
    log_error("font atlas from '{}' has {} glyphs but no texture, so text "
              "draws nothing and measures by estimate. Use a smaller font "
              "size or fewer codepoints.",
              what ? what : "", font.glyphCount);
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
  if (font.texture.id == 0 && font_gl_ready())
    font = build_font_atlas(file, px, cps.data(), (int)cps.size());
  prepare_font_texture(font);
  record_font_coverage(font, file);
  check_font_atlas(font, file);
  return font;
}

inline raylib::Font load_font_from_memory(const unsigned char *data, int size,
                                          const char *name, int px = 32) {
  std::vector<int> cps = default_codepoints();
  raylib::Font font = raylib::LoadFontFromMemory(
      ".ttf", data, size, px, cps.data(), static_cast<int>(cps.size()));
  if (font.glyphCount <= 0) {
    log_warn("Failed to load font from memory: {}", name ? name : "");
    return font;
  }
  prepare_font_texture(font);
  font_coverage::Ranges ranges;
  if (font_coverage::parse_cmap(data, static_cast<size_t>(size), ranges))
    coverage_registry()[font_key(font)] = std::move(ranges);
  check_font_atlas(font, name ? name : "");
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
    warn_once(0x110000 + cp,
              "font has no glyph for U+{:04X}, which draws as a replacement "
              "glyph. Load the font with codepoints for the ranges you use.",
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
    return get_default_font();
  }
  raylib::Font font = raylib::LoadFontEx(file, size, codepoints, codepoint_count);
  // See load_font_from_file: no GL context means no texture.
  if (font.texture.id == 0 && font_gl_ready())
    font = build_font_atlas(file, size, codepoints, codepoint_count);
  prepare_font_texture(font);
  record_font_coverage(font, file);
  check_font_atlas(font, file);
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
  measure_memo::invalidate();
  raylib::UnloadFont(font);
  font = raylib::Font{};
}

inline raylib::Font load_font_for_string(const std::string &content,
                                         const std::string &font_filename,
                                         int size = 96) {
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
  check_font_atlas(font, font_filename.c_str());

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
