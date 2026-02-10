#pragma once

// Shared Metal/Sokol state referenced by both metal_backend.h and font_helper.h.
// This header is kept minimal — only the state needed across modules.

#ifdef AFTER_HOURS_USE_METAL

#include <fontstash/fontstash.h>
#include <cstdint>
#include <functional>

namespace afterhours::graphics::metal_detail {

// ── Application callbacks (set by RunConfig) ──
inline std::function<void()> g_init_fn;
inline std::function<void()> g_frame_fn;
inline std::function<void()> g_cleanup_fn;

// ── Timing ──
inline uint64_t g_start_time = 0;

// ── Font state ──
inline FONScontext* g_fons_ctx = nullptr;
static constexpr int MAX_FONTS = 16;
inline int g_font_ids[MAX_FONTS] = {};
inline int g_font_count = 0;
inline int g_active_font = FONS_INVALID;

// ── Rendering state ──
inline bool g_initialized = false;

}  // namespace afterhours::graphics::metal_detail

#endif  // AFTER_HOURS_USE_METAL
