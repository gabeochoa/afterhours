
#pragma once

#include "../../font_coverage.h"
#include "../../measure_memo.h"

#include <algorithm>
#include <cmath>
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

inline void record_coverage(int font_id, const char *file) {
  font_coverage::Ranges ranges;
  if (font_coverage::parse_file(file, ranges))
    graphics::metal_detail::g_font_coverage[font_id] = std::move(ranges);
}

inline void mesh_font_fallbacks(FONScontext *ctx, int new_id);

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
    record_coverage(id, file);
    // Font ids belong to one fontstash context; a new context resets
    // tracking, since ids from a dead context are meaningless in a new
    // one. Production never recreates the context; tests do.
    if (ctx != graphics::metal_detail::g_font_fallback_ctx) {
      graphics::metal_detail::g_font_fallback_ctx = ctx;
      graphics::metal_detail::g_font_fallback_pairs.clear();
      graphics::metal_detail::g_font_count = 0;
      graphics::metal_detail::g_active_font = FONS_INVALID;
    }
    // Track loaded fonts and set the first one as active default
    auto &md = graphics::metal_detail::g_font_ids;
    if (graphics::metal_detail::g_font_count <
        graphics::metal_detail::MAX_FONTS) {
      md[graphics::metal_detail::g_font_count++] = id;
    }
    if (graphics::metal_detail::g_active_font == FONS_INVALID) {
      graphics::metal_detail::g_active_font = id;
    }
    mesh_font_fallbacks(ctx, id);
  }
  return Font{id};
}

// Fallback chain: every loaded face falls back to every other loaded
// face, in load order, so a glyph missing from one face draws from the
// first loaded face that has it instead of vanishing. Load order is the
// priority: to prefer a symbols face as the fallback, load it first.
// Measurement follows the drawn glyph: fonsTextAdvance resolves through
// the same chain, so drawn and measured widths agree.
// Pairs are registered once via the pair set: fontstash appends without
// dedupe, so a reload would otherwise eat the 20-slot list with repeats.
// This must not read fontstash structs: FONScontext is an incomplete
// type in translation units without FONTSTASH_IMPLEMENTATION.
inline void mesh_font_fallbacks(FONScontext *ctx, int new_id) {
  if (!ctx)
    return;
  auto &pairs = graphics::metal_detail::g_font_fallback_pairs;
  const auto &md = graphics::metal_detail::g_font_ids;
  const int count = graphics::metal_detail::g_font_count;
  for (int i = 0; i < count; i++) {
    const int other = md[i];
    if (other == new_id)
      continue;
    if (pairs.insert({new_id, other}).second)
      fonsAddFallbackFont(ctx, new_id, other);
    if (pairs.insert({other, new_id}).second)
      fonsAddFallbackFont(ctx, other, new_id);
  }
}

// Grow the atlas when fontstash reports it full. fontstash retries the
// failed glyph after the error callback, so a successful expansion
// makes the glyph draw instead of vanishing. Doubling caps at 4096: a
// 4096x4096 R8 atlas is 16 MB, and a glyph that still cannot fit is
// fragmentation, not capacity (resetting would drop every cached glyph
// and corrupt in-flight quads, so it is not the recovery).
inline constexpr int kFontAtlasMaxSize = 4096;

inline bool expand_font_atlas(FONScontext *ctx) {
  if (!ctx)
    return false;
  int w = 0, h = 0;
  fonsGetAtlasSize(ctx, &w, &h);
  if (w <= 0 || h <= 0 || (w >= kFontAtlasMaxSize && h >= kFontAtlasMaxSize))
    return false;
  const int nw = std::min(w * 2, kFontAtlasMaxSize);
  const int nh = std::min(h * 2, kFontAtlasMaxSize);
  if (!fonsExpandAtlas(ctx, nw, nh))
    return false;
  log_info("font atlas expanded to {}x{}; glyphs that did not fit now draw",
           nw, nh);
  return true;
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

// Advance width, same as measure_text.

inline Vector2Type measure_text(const Font font, const char *text,
                                const float size, const float /*spacing*/) {
  if (!std::isfinite(size) || size <= 0.f)
    return Vector2Type{0, 0};
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
  const auto key =
      measure_memo::key_for(text ? text : "", size, 1.f, font_id);
  Vector2Type cached{};
  if (measure_memo::lookup(key, cached))
    return cached;
  fonsSetFont(ctx, fid);
  // Measure at DPI-scaled size (matching draw_text_ex) and convert back.
  float dpi = graphics::metal_detail::dpi_scale();
  fonsSetSize(ctx, size * dpi);
  fonsSetAlign(ctx, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
  // Advance, not the ink box: ink drops the side bearings, so text sized from
  // it comes up short and clips.
  float w = fonsTextAdvance(ctx, text, nullptr) / dpi;
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
