#pragma once

#ifdef AFTER_HOURS_USE_METAL

#include <cstdint>
#include <fontstash/fontstash.h>
#include <functional>
// For sapp_dpi_scale/sapp_width/sapp_height used by the shim accessors below.
// Declarations only (no SOKOL_IMPL); include-guard makes this safe to repeat.
#include <sokol/sokol_app.h>

namespace afterhours::graphics::metal_detail {

// ── Headless (windowless) rendering state ──
// When true, there is no sokol_app: no window, no swapchain, no WindowServer.
// The backend renders into an offscreen render texture instead (see backend.h /
// metal_init). The accessors below feed logical-pixel size/scale to the text
// and drawing code that would otherwise ask sokol_app.
inline bool g_headless = false;
inline int g_headless_w = 0;
inline int g_headless_h = 0;

// Device-pixel scale. Headless renders 1:1 (logical == device pixels).
inline float dpi_scale() { return g_headless ? 1.0f : sapp_dpi_scale(); }
// Framebuffer size in device pixels. Headless has no swapchain, so use the
// configured render-texture size (which we render at dpi=1).
inline int screen_w() { return g_headless ? g_headless_w : sapp_width(); }
inline int screen_h() { return g_headless ? g_headless_h : sapp_height(); }

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

inline bool g_in_texture_mode = false;
inline bool g_pass_active = false;
inline uint32_t g_active_rt_color_view_id = 0;
inline unsigned int g_window_state_flags = 0;

struct ShaderRuntimeState {
  bool ps1_enabled = false;
  float resolution[2] = {0.0f, 0.0f};
};
inline ShaderRuntimeState g_shader_runtime;

} // namespace afterhours::graphics::metal_detail

#endif // AFTER_HOURS_USE_METAL
