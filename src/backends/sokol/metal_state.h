#pragma once

#ifdef AFTER_HOURS_USE_METAL

#include <cstdint>
#include <fontstash/fontstash.h>
#include <functional>

namespace afterhours::graphics::metal_detail {

inline std::function<void()> g_init_fn;
inline std::function<void()> g_frame_fn;
inline std::function<void()> g_cleanup_fn;

inline uint64_t g_start_time = 0;

inline FONScontext *g_fons_ctx = nullptr;
static constexpr int MAX_FONTS = 16;
inline int g_font_ids[MAX_FONTS] = {};
inline int g_font_count = 0;
inline int g_active_font = FONS_INVALID;

inline bool g_initialized = false;

} // namespace afterhours::graphics::metal_detail

#endif // AFTER_HOURS_USE_METAL
