
#pragma once

#include "../../font_coverage.h"
#include "../../measure_memo.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../../developer.h"
#include "metal_state.h"

namespace afterhours {

// Font is an int (fontstash font ID). We wrap it so the type is distinct.
struct Font {
  int id = FONS_INVALID;
};

inline void record_coverage(int font_id, const char *file) {
  font_coverage::Ranges ranges;
  if (font_coverage::parse_file(file, ranges))
    graphics::metal_detail::g_font_coverage[font_id] = std::move(ranges);
}

inline Font track_loaded_font(int id) {
  auto &md = graphics::metal_detail::g_font_ids;
  if (graphics::metal_detail::g_font_count <
      graphics::metal_detail::MAX_FONTS) {
    md[graphics::metal_detail::g_font_count++] = id;
  }
  if (graphics::metal_detail::g_active_font == FONS_INVALID) {
    graphics::metal_detail::g_active_font = id;
  }
  return Font{id};
}

inline Font load_font_from_file(const char *file, int = 0) {
  auto *ctx = graphics::metal_detail::g_fons_ctx;
  if (!ctx) {
    log_warn("fontstash context not ready yet (load_font_from_file)");
    return Font{FONS_INVALID};
  }
  int id = fonsAddFont(ctx, file, file);
  if (id == FONS_INVALID) {
    log_warn("Failed to load font: {}", file);
    return Font{id};
  }
  record_coverage(id, file);
  return track_loaded_font(id);
}

// One face of a font file, by index, so a collection (.ttc) loads: most
// system CJK faces ship as collections and load_font_from_file reads only a
// plain font. A plain font is face 0. Size and codepoints are for raylib,
// which bakes glyphs at load; fontstash rasterises on demand and ignores
// them.
inline Font load_font_face(const char *file, int face_index, int = 0,
                           const std::vector<int> & = {}) {
  auto *ctx = graphics::metal_detail::g_fons_ctx;
  if (!ctx) {
    log_warn("fontstash context not ready yet (load_font_face)");
    return Font{FONS_INVALID};
  }
  std::vector<unsigned char> bytes;
  if (!font_coverage::read_file(file, bytes)) {
    log_warn("Failed to read font: {}", file ? file : "(null)");
    return Font{FONS_INVALID};
  }
  std::vector<unsigned char> face =
      font_coverage::extract_face(bytes, face_index);
  bytes = {};
  if (face.empty()) {
    log_warn("No face {} in font: {}", face_index, file);
    return Font{FONS_INVALID};
  }
  font_coverage::Ranges ranges;
  const bool have_ranges =
      font_coverage::parse_cmap(face.data(), face.size(), ranges);
  // fontstash keeps the bytes and frees them with free().
  auto *data = static_cast<unsigned char *>(std::malloc(face.size()));
  if (!data)
    return Font{FONS_INVALID};
  std::memcpy(data, face.data(), face.size());
  const std::string name = std::string(file) + "#" + std::to_string(face_index);
  const int id = fonsAddFontMem(ctx, name.c_str(), data,
                                static_cast<int>(face.size()), 1);
  if (id == FONS_INVALID) {
    log_warn("Failed to load font face {} of {}", face_index, file);
    return Font{id};
  }
  if (have_ranges)
    graphics::metal_detail::g_font_coverage[id] = std::move(ranges);
  return track_loaded_font(id);
}

// Glyphs `primary` lacks are measured and drawn from the first face in
// `chain` that has them; the chain replaces any earlier one. fontstash does
// the lookup per glyph in both fonsTextAdvance and fonsDrawText, so the two
// agree by construction. False if a face could not be added (invalid, the
// primary itself, or past FONS_MAX_FALLBACKS); the valid rest still apply.
inline bool set_font_fallbacks(const Font primary,
                               const std::vector<Font> &chain) {
  auto *ctx = graphics::metal_detail::g_fons_ctx;
  if (!ctx || primary.id == FONS_INVALID)
    return false;
  fonsResetFallbackFont(ctx, primary.id);
  std::vector<int> ids;
  bool ok = true;
  for (const Font &f : chain) {
    if (f.id == FONS_INVALID || f.id == primary.id ||
        !fonsAddFallbackFont(ctx, primary.id, f.id)) {
      ok = false;
      continue;
    }
    ids.push_back(f.id);
  }
  graphics::metal_detail::g_font_fallbacks[primary.id] = std::move(ids);
  measure_memo::invalidate();
  return ok;
}

inline std::vector<Font> font_fallbacks(const Font primary) {
  std::vector<Font> out;
  const auto &chains = graphics::metal_detail::g_font_fallbacks;
  if (const auto it = chains.find(primary.id); it != chains.end())
    for (int id : it->second)
      out.push_back(Font{id});
  return out;
}

namespace font_detail {
// Recorded cmap for one id; no record means unknown, which counts as covered.
inline bool face_covers(int fid, uint32_t cp) {
  const auto &cov = graphics::metal_detail::g_font_coverage;
  const auto it = cov.find(fid);
  return it == cov.end() || font_coverage::covers(it->second, cp);
}

inline bool chain_covers(int fid, uint32_t cp) {
  if (face_covers(fid, cp))
    return true;
  const auto &chains = graphics::metal_detail::g_font_fallbacks;
  if (const auto it = chains.find(fid); it != chains.end())
    for (int id : it->second)
      if (face_covers(id, cp))
        return true;
  return false;
}

// `text` with what the chain cannot draw replaced (font_coverage::
// visible_text). measure_text and draw_text_ex both go through here.
inline const char *visible(int fid, const char *text, std::string &scratch) {
  return font_coverage::visible_text(
             text, [fid](uint32_t cp) { return chain_covers(fid, cp); },
             scratch)
             ? scratch.c_str()
             : text;
}
} // namespace font_detail

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

// Advance width, same as measure_text.

inline Vector2Type measure_text(const Font font, const char *text,
                                const float size, const float /*spacing*/) {
  auto *ctx = graphics::metal_detail::g_fons_ctx;
  if (!ctx)
    return Vector2Type{0, 0};
  int fid = (font.id != FONS_INVALID) ? font.id
                                      : graphics::metal_detail::g_active_font;
  if (fid == FONS_INVALID)
    return Vector2Type{0, 0};
  // dpi is part of the answer, so it is part of the key.
  const float dpi_now = graphics::metal_detail::dpi_scale();
  const std::uint64_t font_id = (static_cast<std::uint64_t>(fid) << 32) ^
                                static_cast<std::uint64_t>(dpi_now * 1024.f);
  const std::uint64_t key =
      measure_memo::hash(text ? text : "", size, 1.f, font_id);
  Vector2Type cached{};
  if (measure_memo::lookup(key, cached))
    return cached;
  fonsSetFont(ctx, fid);
  // Measure at DPI-scaled size (matching draw_text_ex) and convert back.
  float dpi = graphics::metal_detail::dpi_scale();
  fonsSetSize(ctx, size * dpi);
  fonsSetAlign(ctx, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
  std::string scratch;
  // Advance, not the ink box: ink drops the side bearings, so text sized from
  // it comes up short and clips.
  float w = fonsTextAdvance(ctx, font_detail::visible(fid, text, scratch),
                            nullptr) /
            dpi;
  float ascender, descender, lineh;
  fonsVertMetrics(ctx, &ascender, &descender, &lineh);
  const Vector2Type measured{w, lineh / dpi};
  measure_memo::store(key, measured);
  return measured;
}

inline Vector2Type measure_text_utf8(const Font font, const char *text,
                                     const float size, const float spacing) {
  return measure_text(font, text, size, spacing);
}

inline float get_first_glyph_bearing(const Font, const char *) { return 0.0f; }

// Distance from the top of a drawn line to its baseline, in logical pixels.
inline float font_baseline(const Font font, const float size) {
  auto *ctx = graphics::metal_detail::g_fons_ctx;
  const int fid = (font.id != FONS_INVALID)
                      ? font.id
                      : graphics::metal_detail::g_active_font;
  if (!ctx || fid == FONS_INVALID)
    return size * 0.8f;
  const float dpi = graphics::metal_detail::dpi_scale();
  fonsSetFont(ctx, fid);
  fonsSetSize(ctx, size * dpi);
  float ascender = 0, descender = 0, lineh = 0;
  fonsVertMetrics(ctx, &ascender, &descender, &lineh);
  return ascender / dpi;
}

inline bool is_font_loaded(const Font &font) { return font.id != FONS_INVALID; }

// Whether the face can draw a codepoint, from the cmap recorded at load.
// A font with no recorded cmap (added to fontstash directly, not through
// load_font_from_file) reports true: unknown is not missing.
inline bool font_has_glyph(const Font font, int codepoint) {
  if (codepoint < 0)
    return false;
  auto *ctx = graphics::metal_detail::g_fons_ctx;
  if (!ctx)
    return false;
  const int fid = (font.id != FONS_INVALID)
                      ? font.id
                      : graphics::metal_detail::g_active_font;
  if (fid == FONS_INVALID)
    return false;
  const auto it = graphics::metal_detail::g_font_coverage.find(fid);
  if (it == graphics::metal_detail::g_font_coverage.end())
    return true;
  return font_coverage::covers(it->second,
                               static_cast<uint32_t>(codepoint));
}

} // namespace afterhours
