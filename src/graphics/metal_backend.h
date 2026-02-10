// vendor/afterhours/src/graphics/metal_backend.h
// Metal/Sokol backend stub -- satisfies PlatformBackend concept with no-ops.
//
// To use: #define AFTER_HOURS_USE_METAL (instead of AFTER_HOURS_USE_RAYLIB)
// Prerequisites: vendor sokol headers (sokol_app.h, sokol_gfx.h, etc.)

#pragma once

#ifdef AFTER_HOURS_USE_METAL

#include "graphics_concept.h"
#include "../plugins/color.h"

namespace afterhours::graphics {

struct MetalPlatformAPI {
    using color_type = afterhours::Color;

    // ── Constants ──
    static constexpr unsigned int FLAG_WINDOW_RESIZABLE = 0x00000004;
    static constexpr int LOG_ERROR = 5;
    static constexpr int TEXTURE_FILTER_BILINEAR = 1;

    // ── Window lifecycle ──
    static void init_window(int, int, const char*) {
        // TODO: sokol_app init (SOKOL_NO_ENTRY mode)
    }
    static void close_window() {
        // TODO: sokol_gfx shutdown
    }
    static bool window_should_close() {
        // TODO: check sokol quit request
        return false;
    }
    static bool is_window_ready() { return true; }
    static bool is_window_fullscreen() { return false; }
    static void toggle_fullscreen() { /* TODO */ }
    static void minimize_window() { /* TODO */ }

    // ── Config ──
    static void set_config_flags(unsigned int) { /* TODO */ }
    static void set_target_fps(int) { /* TODO */ }
    static void set_exit_key(int) { /* TODO */ }
    static void set_trace_log_level(int) { /* TODO */ }

    // ── Frame ──
    static void begin_drawing() { /* TODO: sg_begin_pass */ }
    static void end_drawing() { /* TODO: sg_end_pass + sg_commit */ }
    static void clear_background(afterhours::Color) { /* TODO */ }

    // ── Screen / timing ──
    static int get_screen_width() { return 1280; /* TODO: sapp_width() */ }
    static int get_screen_height() { return 720; /* TODO: sapp_height() */ }
    static float get_frame_time() { return 1.0f / 60.0f; /* TODO: sapp_frame_duration() */ }
    static float get_fps() { return 60.0f; }
    static double get_time() { return 0.0; /* TODO: stm_sec(stm_now()) */ }

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
};

static_assert(PlatformBackend<MetalPlatformAPI>,
              "MetalPlatformAPI must satisfy PlatformBackend concept");

}  // namespace afterhours::graphics

#endif  // AFTER_HOURS_USE_METAL
