#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../logging.h"
#include "psx_profile.h"
#include "vram_allocator.h"

// The PSX budget tracker: one simulated VRAM plus the running totals a game
// would want to watch while aiming at a console it is not running on.
//
// Usage from a game (any backend):
//
//   psx::budget::configure({.console = psx::Console::PS1});
//   ... load textures, render frames ...
//   log_info("{}", psx::budget::report_string());
//
// The PSX backend (drawing_helpers.h in this directory) calls into here from
// its texture/render-target functions, so a game built with
// AFTER_HOURS_USE_PSX gets the accounting for free. The tracker itself does
// not depend on the backend: a raylib/sokol game can call alloc_texture /
// free_texture around its own loads to get the same warnings today.
//
// Warnings, all through log_warn so AFTER_HOURS_REPLACE_LOGGING hosts see
// them too:
//   * a texture larger than the console can upload in one piece,
//   * a texture that does not fit in the VRAM that remains,
//   * VRAM crossing the warn_at watermark (once per crossing),
//   * a frame whose primitive count exceeds the profile budget.

namespace afterhours::psx::budget {

struct BudgetConfig {
  Console console = Console::PS1;
  // Full profile override; when set, `console` is ignored. Use this for
  // "what if we only had half the VRAM" experiments.
  std::optional<ConsoleProfile> profile_override;
  // Render setup override; when set, replaces the profile's render setup
  // (and therefore its framebuffer reservation).
  std::optional<RenderSetup> render_override;
  float warn_at = 0.85f;
  // Strict: a texture that does not fit (or exceeds the console's maximum
  // texture size) reports ok=false, and the PSX backend refuses to hand
  // back a texture for it. Non-strict: the alloc is still recorded as far
  // as it can be placed, so reports stay truthful, and the game keeps
  // running. Non-strict overflow that cannot be placed at all tracks the
  // bytes as overspill (see Report::overspill_bytes) instead of placing.
  bool strict = false;
  // Bit depth for allocs that do not state one. 0 = profile default.
  int texture_bpp = 0;
  bool reserve_framebuffers = true;
};

struct TextureAlloc {
  std::uint64_t id = 0; // pass back to free_texture; 0 = untracked
  bool ok = false;      // placed in VRAM (and, in strict mode, legal at all)
  bool oversized = false; // exceeds the console's max texture dimensions
  VramRect rect;          // image placement, invalid when !ok
  VramRect clut_rect;     // palette placement, invalid when none
  int width_px = 0;
  int height_px = 0;
  int bpp = 0;
  std::size_t bytes = 0; // image + CLUT, as placed
};

struct Report {
  const char *profile_name = "";
  std::size_t vram_total_bytes = 0;
  std::size_t vram_used_bytes = 0;
  std::size_t vram_free_bytes = 0;
  float vram_fraction_used = 0.f;
  std::size_t framebuffer_bytes = 0;
  // Bytes of textures that were requested but could not be placed
  // (non-strict mode). This is the "how far over are we" number.
  std::size_t overspill_bytes = 0;
  std::size_t texture_count = 0;
  std::size_t render_target_count = 0;
  std::size_t failed_alloc_count = 0;
  std::size_t oversized_alloc_count = 0;
  std::size_t primitives_last_frame = 0;
  std::size_t primitives_peak_frame = 0;
  std::size_t frames_over_primitive_budget = 0;
};

namespace detail {

struct TrackedAlloc {
  VramRect rect;
  VramRect clut_rect;
  std::size_t bytes = 0;
  bool is_render_target = false;
  std::string name;
};

struct BudgetState {
  BudgetConfig config;
  ConsoleProfile profile = ps1_profile();
  VramAllocator vram;
  std::unordered_map<std::uint64_t, TrackedAlloc> allocs;
  std::uint64_t next_id = 1;
  std::size_t framebuffer_bytes = 0;
  std::size_t overspill_bytes = 0;
  std::size_t texture_count = 0;
  std::size_t render_target_count = 0;
  std::size_t failed_alloc_count = 0;
  std::size_t oversized_alloc_count = 0;
  std::size_t primitives_this_frame = 0;
  std::size_t primitives_last_frame = 0;
  std::size_t primitives_peak_frame = 0;
  std::size_t frames_over_primitive_budget = 0;
  bool configured = false;
  bool warned_watermark = false;
};

inline BudgetState &state() {
  static BudgetState s;
  return s;
}

inline void ensure_configured();

// Reserve framebuffers for the profile's render setup. First-fit from the
// top-left, which puts them where both consoles conventionally lay them out
// and stacks the second buffer below the first when they cannot sit side by
// side (PS2 at 640x448 @32bpp, PS1 interlaced modes). A render setup whose
// framebuffers cannot fit at all warns and runs with what did fit -- PS1
// 640x480 double-buffered is 1.2MB into 1MB of VRAM, which is precisely the
// kind of thing this exists to say out loud.
inline void reserve_framebuffers(BudgetState &s) {
  s.framebuffer_bytes = 0;
  if (!s.config.reserve_framebuffers)
    return;
  const RenderSetup &r = s.profile.render;
  const int w = units_for_row(r.width, r.color_depth_bpp);
  const int buffers = r.double_buffered ? 2 : 1;
  for (int i = 0; i < buffers; ++i) {
    if (auto fb = s.vram.allocate(w, r.height)) {
      s.framebuffer_bytes += fb->bytes();
    } else {
      log_warn("psx budget: {} framebuffer {}x{} @{}bpp does not fit in "
               "VRAM alongside the previous reservations",
               s.profile.name, r.width, r.height, r.color_depth_bpp);
      break;
    }
  }
}

inline void apply_config(BudgetState &s) {
  s.profile = s.config.profile_override.has_value()
                  ? *s.config.profile_override
                  : profile_for(s.config.console);
  if (s.config.render_override.has_value())
    s.profile.render = *s.config.render_override;
  s.vram.reset(s.profile.vram_width_px, s.profile.vram_height_px);
  s.allocs.clear();
  s.next_id = 1;
  s.overspill_bytes = 0;
  s.texture_count = 0;
  s.render_target_count = 0;
  s.failed_alloc_count = 0;
  s.oversized_alloc_count = 0;
  s.primitives_this_frame = 0;
  s.primitives_last_frame = 0;
  s.primitives_peak_frame = 0;
  s.frames_over_primitive_budget = 0;
  s.warned_watermark = false;
  reserve_framebuffers(s);
  s.configured = true;
}

inline void ensure_configured() {
  BudgetState &s = state();
  if (!s.configured)
    apply_config(s);
}

inline void check_watermark(BudgetState &s) {
  const float frac = s.vram.fraction_used();
  if (frac >= s.config.warn_at && !s.warned_watermark) {
    s.warned_watermark = true;
    log_warn("psx budget: {} VRAM {}% full ({}KB / {}KB used, {}KB "
             "framebuffers, {} textures)",
             s.profile.name, static_cast<int>(frac * 100.f + 0.5f),
             s.vram.used_bytes() / 1024, s.vram.total_bytes() / 1024,
             s.framebuffer_bytes / 1024, s.texture_count);
  } else if (frac < s.config.warn_at) {
    s.warned_watermark = false;
  }
}

inline TextureAlloc alloc_impl(int width_px, int height_px, int bpp,
                               const char *name, bool is_render_target) {
  BudgetState &s = state();
  ensure_configured();

  TextureAlloc out;
  out.width_px = width_px;
  out.height_px = height_px;
  out.bpp = bpp > 0 ? bpp
            : s.config.texture_bpp > 0
                ? s.config.texture_bpp
                : (is_render_target ? s.profile.render.color_depth_bpp
                                    : s.profile.default_texture_bpp);
  out.bytes = texture_bytes(width_px, height_px, out.bpp);

  const char *label = name != nullptr && name[0] != '\0' ? name : "<unnamed>";

  if (width_px <= 0 || height_px <= 0) {
    log_warn("psx budget: ignoring {}x{} texture '{}'", width_px, height_px,
             label);
    return out;
  }

  out.oversized = !is_render_target &&
                  (width_px > s.profile.max_texture_width ||
                   height_px > s.profile.max_texture_height);
  if (out.oversized) {
    ++s.oversized_alloc_count;
    log_warn("psx budget: texture '{}' is {}x{}, over the {} maximum "
             "texture size of {}x{} -- split it or atlas it",
             label, width_px, height_px, s.profile.name,
             s.profile.max_texture_width, s.profile.max_texture_height);
    if (s.config.strict)
      return out;
  }

  // CLUT first: it is one short row, and placing it before the image keeps
  // the image's first-fit scan from fragmenting around it.
  if (s.profile.textures_use_clut && !is_render_target) {
    const int clut = clut_units(out.bpp);
    if (clut > 0) {
      if (auto rect = s.vram.allocate(clut, 1)) {
        out.clut_rect = *rect;
        out.bytes += rect->bytes();
      }
    }
  }

  auto placed =
      s.vram.allocate(units_for_row(width_px, out.bpp), height_px);
  if (!placed.has_value()) {
    if (out.clut_rect.valid()) {
      s.vram.free(out.clut_rect);
      out.clut_rect = {};
    }
    ++s.failed_alloc_count;
    s.overspill_bytes += out.bytes;
    log_warn("psx budget: texture '{}' ({}x{} @{}bpp = {}KB) does not fit "
             "in {} VRAM -- {}KB free of {}KB",
             label, width_px, height_px, out.bpp, out.bytes / 1024,
             s.profile.name, s.vram.free_bytes() / 1024,
             s.vram.total_bytes() / 1024);
    return out;
  }

  out.rect = *placed;
  out.ok = true;
  out.id = s.next_id++;
  s.allocs[out.id] = TrackedAlloc{.rect = out.rect,
                                  .clut_rect = out.clut_rect,
                                  .bytes = out.bytes,
                                  .is_render_target = is_render_target,
                                  .name = label};
  if (is_render_target)
    ++s.render_target_count;
  else
    ++s.texture_count;
  check_watermark(s);
  return out;
}

} // namespace detail

inline void configure(const BudgetConfig &config) {
  detail::BudgetState &s = detail::state();
  s.config = config;
  detail::apply_config(s);
}

// Back to the configured profile with every texture freed and framebuffers
// re-reserved. The backend calls this on shutdown; tests call it between
// cases.
inline void reset() { detail::apply_config(detail::state()); }

inline const ConsoleProfile &profile() {
  detail::ensure_configured();
  return detail::state().profile;
}

inline const BudgetConfig &config() { return detail::state().config; }

// Track one texture upload. bpp 0 = config/profile default.
inline TextureAlloc alloc_texture(int width_px, int height_px,
                                  const char *name, int bpp = 0) {
  return detail::alloc_impl(width_px, height_px, bpp, name, false);
}

// Track one render target, charged at the render setup's color depth unless
// bpp says otherwise.
inline TextureAlloc alloc_render_target(int width_px, int height_px,
                                        const char *name, int bpp = 0) {
  return detail::alloc_impl(width_px, height_px, bpp, name, true);
}

inline void free_texture(std::uint64_t id) {
  if (id == 0)
    return;
  detail::BudgetState &s = detail::state();
  auto it = s.allocs.find(id);
  if (it == s.allocs.end())
    return;
  s.vram.free(it->second.rect);
  if (it->second.clut_rect.valid())
    s.vram.free(it->second.clut_rect);
  if (it->second.is_render_target && s.render_target_count > 0)
    --s.render_target_count;
  else if (!it->second.is_render_target && s.texture_count > 0)
    --s.texture_count;
  s.allocs.erase(it);
  detail::check_watermark(s);
}

inline void add_primitives(std::size_t count) {
  detail::ensure_configured();
  detail::state().primitives_this_frame += count;
}

// Close out a frame: fold the primitive count into last/peak and warn when
// the frame ran over the profile's primitive budget.
inline void end_frame() {
  detail::BudgetState &s = detail::state();
  detail::ensure_configured();
  s.primitives_last_frame = s.primitives_this_frame;
  if (s.primitives_this_frame > s.primitives_peak_frame)
    s.primitives_peak_frame = s.primitives_this_frame;
  const std::size_t budget_prims = s.profile.primitive_budget_per_frame;
  if (budget_prims > 0 && s.primitives_this_frame > budget_prims) {
    ++s.frames_over_primitive_budget;
    log_warn("psx budget: frame drew {} primitives, over the {} budget of "
             "{} per frame",
             s.primitives_this_frame, s.profile.name, budget_prims);
  }
  s.primitives_this_frame = 0;
}

inline Report report() {
  detail::BudgetState &s = detail::state();
  detail::ensure_configured();
  Report r;
  r.profile_name = s.profile.name;
  r.vram_total_bytes = s.vram.total_bytes();
  r.vram_used_bytes = s.vram.used_bytes();
  r.vram_free_bytes = s.vram.free_bytes();
  r.vram_fraction_used = s.vram.fraction_used();
  r.framebuffer_bytes = s.framebuffer_bytes;
  r.overspill_bytes = s.overspill_bytes;
  r.texture_count = s.texture_count;
  r.render_target_count = s.render_target_count;
  r.failed_alloc_count = s.failed_alloc_count;
  r.oversized_alloc_count = s.oversized_alloc_count;
  r.primitives_last_frame = s.primitives_last_frame;
  r.primitives_peak_frame = s.primitives_peak_frame;
  r.frames_over_primitive_budget = s.frames_over_primitive_budget;
  return r;
}

// One-line human summary, e.g.
//   PlayStation 1 VRAM 612KB/1024KB (60%), 300KB framebuffers, 23 textures
inline std::string report_string() {
  const Report r = report();
  std::string out = std::string(r.profile_name) + " VRAM " +
                    std::to_string(r.vram_used_bytes / 1024) + "KB/" +
                    std::to_string(r.vram_total_bytes / 1024) + "KB (" +
                    std::to_string(
                        static_cast<int>(r.vram_fraction_used * 100.f + 0.5f)) +
                    "%), " + std::to_string(r.framebuffer_bytes / 1024) +
                    "KB framebuffers, " + std::to_string(r.texture_count) +
                    " textures";
  if (r.overspill_bytes > 0)
    out += ", " + std::to_string(r.overspill_bytes / 1024) + "KB OVER BUDGET";
  return out;
}

inline std::string vram_map_ascii(int cols = 64, int rows = 32) {
  detail::ensure_configured();
  return detail::state().vram.map_ascii(cols, rows);
}

// Names of every live allocation, largest first -- the "who is eating VRAM"
// list to walk when the watermark warning fires.
inline std::vector<std::string> alloc_listing() {
  detail::BudgetState &s = detail::state();
  detail::ensure_configured();
  std::vector<const detail::TrackedAlloc *> live;
  live.reserve(s.allocs.size());
  for (const auto &kv : s.allocs)
    live.push_back(&kv.second);
  std::sort(live.begin(), live.end(),
            [](const detail::TrackedAlloc *a, const detail::TrackedAlloc *b) {
              return a->bytes > b->bytes;
            });
  std::vector<std::string> out;
  out.reserve(live.size());
  for (const detail::TrackedAlloc *a : live)
    out.push_back(a->name + " (" + std::to_string(a->bytes / 1024) + "KB)" +
                  (a->is_render_target ? " [render target]" : ""));
  return out;
}

} // namespace afterhours::psx::budget
