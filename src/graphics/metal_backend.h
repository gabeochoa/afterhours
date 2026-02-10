// vendor/afterhours/src/graphics/metal_backend.h
// Metal/Sokol backend -- satisfies PlatformBackend concept.
//
// To use: #define AFTER_HOURS_USE_METAL (instead of AFTER_HOURS_USE_RAYLIB)
// Requires: sokol headers vendored in vendor/sokol/

#pragma once

#ifdef AFTER_HOURS_USE_METAL

#include "graphics_concept.h"
#include "../plugins/color.h"

// ── Sokol headers (declaration only, no implementation) ──
// The implementation must be compiled in exactly one .cpp/.mm file
// with SOKOL_IMPL defined before including these headers.
#include <sokol/sokol_app.h>
#include <sokol/sokol_gfx.h>
#include <sokol/sokol_glue.h>
#include <sokol/sokol_time.h>
#include <sokol/sokol_log.h>

namespace afterhours::graphics {

namespace metal_detail {
    // Global state for the sokol callback bridge.
    // Sokol uses C callbacks so we store the user's std::functions here.
    inline std::function<void()> g_init_fn;
    inline std::function<void()> g_frame_fn;
    inline std::function<void()> g_cleanup_fn;
    inline sg_pass_action g_pass_action{};
    inline bool g_initialized = false;
    inline uint64_t g_start_time = 0;

    inline void sokol_init_cb() {
        sg_desc desc{};
        desc.environment = sglue_environment();
        desc.logger.func = slog_func;
        sg_setup(&desc);
        stm_setup();
        g_start_time = stm_now();
        g_initialized = true;

        // Default clear to black
        g_pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
        g_pass_action.colors[0].clear_value = {0.0f, 0.0f, 0.0f, 1.0f};

        if (g_init_fn) g_init_fn();
    }

    inline void sokol_frame_cb() {
        if (g_frame_fn) g_frame_fn();
    }

    inline void sokol_cleanup_cb() {
        if (g_cleanup_fn) g_cleanup_fn();
        sg_shutdown();
        g_initialized = false;
    }

    inline void sokol_event_cb(const sapp_event* /*ev*/) {
        // TODO: translate sokol events to afterhours input system
    }
}  // namespace metal_detail

struct MetalPlatformAPI {
    using color_type = afterhours::Color;

    // ── Constants ──
    static constexpr unsigned int FLAG_WINDOW_RESIZABLE = 0x00000004;
    static constexpr int LOG_ERROR = 5;
    static constexpr int TEXTURE_FILTER_BILINEAR = 1;

    // ── Window lifecycle (legacy API -- prefer run()) ──
    static void init_window(int, int, const char*) {
        // Under Metal, window creation happens inside sapp_run().
        // This is a no-op; use run() instead.
    }
    static void close_window() {
        // Handled by sokol cleanup callback.
    }
    static bool window_should_close() {
        // Sokol owns the event loop; this is only meaningful
        // inside a frame callback. Always returns false.
        return false;
    }
    static bool is_window_ready() { return metal_detail::g_initialized; }
    static bool is_window_fullscreen() { return sapp_is_fullscreen(); }
    static void toggle_fullscreen() { sapp_toggle_fullscreen(); }
    static void minimize_window() { /* not supported by sokol */ }

    // ── Config (legacy API -- prefer RunConfig fields) ──
    static void set_config_flags(unsigned int) { /* handled via RunConfig.flags */ }
    static void set_target_fps(int) { /* handled via RunConfig.target_fps */ }
    static void set_exit_key(int) { /* TODO: map to sokol event handling */ }
    static void set_trace_log_level(int) { /* TODO */ }

    // ── Frame ──
    static void begin_drawing() {
        sg_pass pass{};
        pass.action = metal_detail::g_pass_action;
        pass.swapchain = sglue_swapchain();
        sg_begin_pass(&pass);
    }

    static void end_drawing() {
        sg_end_pass();
        sg_commit();
    }

    static void clear_background(afterhours::Color c) {
        metal_detail::g_pass_action.colors[0].clear_value = {
            static_cast<float>(c.r) / 255.0f,
            static_cast<float>(c.g) / 255.0f,
            static_cast<float>(c.b) / 255.0f,
            static_cast<float>(c.a) / 255.0f,
        };
    }

    // ── Screen / timing ──
    static int get_screen_width() { return sapp_width(); }
    static int get_screen_height() { return sapp_height(); }
    static float get_frame_time() { return static_cast<float>(sapp_frame_duration()); }
    static float get_fps() {
        float dt = get_frame_time();
        return dt > 0.0f ? 1.0f / dt : 0.0f;
    }
    static double get_time() {
        return stm_sec(stm_since(metal_detail::g_start_time));
    }

    // ── Text measurement ──
    static int measure_text(const char*, int) {
        // TODO: fontstash or stb_truetype
        return 0;
    }

    // ── Screenshots ──
    static void take_screenshot(const char*) {
        // TODO: read Metal framebuffer pixels -> PNG
    }

    // ── Input ──
    static bool is_key_pressed_repeat(int) {
        // TODO: track from sokol_app key events
        return false;
    }

    // ── Unified run loop ──
    static void run(const RunConfig& cfg) {
        metal_detail::g_init_fn = cfg.init;
        metal_detail::g_frame_fn = cfg.frame;
        metal_detail::g_cleanup_fn = cfg.cleanup;

        sapp_desc desc{};
        desc.init_cb = metal_detail::sokol_init_cb;
        desc.frame_cb = metal_detail::sokol_frame_cb;
        desc.cleanup_cb = metal_detail::sokol_cleanup_cb;
        desc.event_cb = metal_detail::sokol_event_cb;
        desc.width = cfg.width;
        desc.height = cfg.height;
        desc.window_title = cfg.title;
        desc.logger.func = slog_func;
        desc.high_dpi = true;

        // Map flags
        if (cfg.flags & FLAG_WINDOW_RESIZABLE) {
            // sokol windows are resizable by default
        }

        // sapp_run blocks until the application quits
        sapp_run(&desc);
    }
};

static_assert(PlatformBackend<MetalPlatformAPI>,
              "MetalPlatformAPI must satisfy PlatformBackend concept");

}  // namespace afterhours::graphics

#endif  // AFTER_HOURS_USE_METAL
