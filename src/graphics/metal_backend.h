// vendor/afterhours/src/graphics/metal_backend.h
// Metal/Sokol backend -- satisfies PlatformBackend concept.
//
// To use: #define AFTER_HOURS_USE_METAL (instead of AFTER_HOURS_USE_RAYLIB)
// Requires: sokol headers vendored in vendor/sokol/

#pragma once

#ifdef AFTER_HOURS_USE_METAL

#include "graphics_concept.h"
#include "../logging.h"

// ── Sokol headers (declaration only, no implementation) ──
// The implementation must be compiled in exactly one .cpp/.mm file
// with SOKOL_IMPL defined before including these headers.
// We use SOKOL_NO_ENTRY so we call sapp_run() ourselves from run().
#ifndef SOKOL_NO_ENTRY
#define SOKOL_NO_ENTRY
#endif

// Sokol headers — resolved via -isystem vendor/afterhours/vendor/
#include <sokol/sokol_app.h>
#include <sokol/sokol_gfx.h>
#include <sokol/sokol_glue.h>
#include <sokol/sokol_time.h>
#include <sokol/sokol_log.h>

// 2D drawing (immediate-mode GL-style API on top of sokol_gfx)
#include <sokol/sokol_gl.h>

// Font rendering (fontstash + sokol integration)
#include <fontstash/fontstash.h>
#include <sokol/sokol_fontstash.h>

// Shared state (also used by font_helper.h)
#include "metal_state.h"

namespace afterhours::graphics {

namespace metal_detail {

    // ── Rendering pass action (local to metal_backend) ──
    inline sg_pass_action g_pass_action{};

    // ── Input state ──
    // Sokol keycodes are GLFW-compatible (0-511 range covers all keys).
    static constexpr int MAX_KEYS = 512;
    static constexpr int MAX_MOUSE_BUTTONS = 4;

    // Per-key state: down tracks held, pressed/released are edge-triggered per frame.
    struct InputState {
        bool key_down[MAX_KEYS]{};
        bool key_pressed[MAX_KEYS]{};      // went down this frame
        bool key_released[MAX_KEYS]{};     // went up this frame
        bool key_repeat[MAX_KEYS]{};       // repeat event this frame

        bool mouse_down[MAX_MOUSE_BUTTONS]{};
        bool mouse_pressed[MAX_MOUSE_BUTTONS]{};
        bool mouse_released[MAX_MOUSE_BUTTONS]{};

        float mouse_x = 0.f;
        float mouse_y = 0.f;
        float mouse_dx = 0.f;
        float mouse_dy = 0.f;
        float scroll_x = 0.f;
        float scroll_y = 0.f;

        // Character input queue (UTF-32 codepoints from CHAR events)
        static constexpr int CHAR_QUEUE_SIZE = 32;
        uint32_t char_queue[CHAR_QUEUE_SIZE]{};
        int char_queue_head = 0;
        int char_queue_tail = 0;
    };

    inline InputState& input_state() {
        static InputState s{};
        return s;
    }

    // Call at the start of each frame to clear edge-triggered state.
    inline void input_begin_frame() {
        auto& s = input_state();
        for (int i = 0; i < MAX_KEYS; i++) {
            s.key_pressed[i] = false;
            s.key_released[i] = false;
            s.key_repeat[i] = false;
        }
        for (int i = 0; i < MAX_MOUSE_BUTTONS; i++) {
            s.mouse_pressed[i] = false;
            s.mouse_released[i] = false;
        }
        s.mouse_dx = 0.f;
        s.mouse_dy = 0.f;
        s.scroll_x = 0.f;
        s.scroll_y = 0.f;
    }

    inline void push_char(uint32_t c) {
        auto& s = input_state();
        int next = (s.char_queue_tail + 1) % InputState::CHAR_QUEUE_SIZE;
        if (next != s.char_queue_head) {
            s.char_queue[s.char_queue_tail] = c;
            s.char_queue_tail = next;
        }
    }

    inline uint32_t pop_char() {
        auto& s = input_state();
        if (s.char_queue_head == s.char_queue_tail) return 0;
        uint32_t c = s.char_queue[s.char_queue_head];
        s.char_queue_head = (s.char_queue_head + 1) % InputState::CHAR_QUEUE_SIZE;
        return c;
    }

    // ── Sokol callbacks ──

    inline void sokol_init_cb() {
        sg_desc desc{};
        desc.environment = sglue_environment();
        desc.logger.func = slog_func;
        sg_setup(&desc);
        stm_setup();
        g_start_time = stm_now();

        // Initialize sokol_gl for 2D drawing (generous buffer for complex UIs)
        sgl_desc_t sgl_desc{};
        sgl_desc.max_vertices = 1 << 18;   // 262144 vertices
        sgl_desc.max_commands = 1 << 16;   // 65536 commands
        sgl_desc.logger.func = slog_func;
        sgl_setup(&sgl_desc);

        // Initialize fontstash for text rendering
        sfons_desc_t sfons_desc{};
        sfons_desc.width = 1024;
        sfons_desc.height = 1024;
        g_fons_ctx = sfons_create(&sfons_desc);

        g_initialized = true;

        g_pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
        g_pass_action.colors[0].clear_value = {0.0f, 0.0f, 0.0f, 1.0f};

        if (g_init_fn) g_init_fn();
    }

    inline void sokol_frame_cb() {
        if (g_frame_fn) g_frame_fn();
        // Clear edge state after the frame callback has had a chance to read it.
        input_begin_frame();
    }

    inline void sokol_cleanup_cb() {
        if (g_cleanup_fn) g_cleanup_fn();
        if (g_fons_ctx) {
            sfons_destroy(g_fons_ctx);
            g_fons_ctx = nullptr;
        }
        sgl_shutdown();
        sg_shutdown();
        g_initialized = false;
    }

    inline void sokol_event_cb(const sapp_event* ev) {
        auto& s = input_state();
        const int kc = static_cast<int>(ev->key_code);

        switch (ev->type) {
        case SAPP_EVENTTYPE_KEY_DOWN:
            if (kc > 0 && kc < MAX_KEYS) {
                if (!s.key_down[kc]) {
                    s.key_pressed[kc] = true;
                }
                s.key_down[kc] = true;
                if (ev->key_repeat) {
                    s.key_repeat[kc] = true;
                }
            }
            break;
        case SAPP_EVENTTYPE_KEY_UP:
            if (kc > 0 && kc < MAX_KEYS) {
                s.key_down[kc] = false;
                s.key_released[kc] = true;
            }
            break;
        case SAPP_EVENTTYPE_CHAR:
            if (ev->char_code > 0) {
                push_char(ev->char_code);
            }
            break;
        case SAPP_EVENTTYPE_MOUSE_DOWN: {
            int btn = static_cast<int>(ev->mouse_button);
            if (btn >= 0 && btn < MAX_MOUSE_BUTTONS) {
                s.mouse_down[btn] = true;
                s.mouse_pressed[btn] = true;
            }
            s.mouse_x = ev->mouse_x;
            s.mouse_y = ev->mouse_y;
            break;
        }
        case SAPP_EVENTTYPE_MOUSE_UP: {
            int btn = static_cast<int>(ev->mouse_button);
            if (btn >= 0 && btn < MAX_MOUSE_BUTTONS) {
                s.mouse_down[btn] = false;
                s.mouse_released[btn] = true;
            }
            s.mouse_x = ev->mouse_x;
            s.mouse_y = ev->mouse_y;
            break;
        }
        case SAPP_EVENTTYPE_MOUSE_MOVE:
            s.mouse_x = ev->mouse_x;
            s.mouse_y = ev->mouse_y;
            s.mouse_dx += ev->mouse_dx;
            s.mouse_dy += ev->mouse_dy;
            break;
        case SAPP_EVENTTYPE_MOUSE_SCROLL:
            s.scroll_x += ev->scroll_x;
            s.scroll_y += ev->scroll_y;
            break;
        default:
            break;
        }
    }
}  // namespace metal_detail

struct MetalPlatformAPI {
    // Lightweight color struct — satisfies ColorLike without pulling in color.h
    struct color_type {
        unsigned char r, g, b, a;
    };

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
    static void minimize_window() { log_error("@notimplemented minimize_window"); }

    // ── Config (legacy API -- prefer RunConfig fields) ──
    static void set_config_flags(unsigned int) { /* handled via RunConfig.flags */ }
    static void set_target_fps(int) { /* handled via RunConfig.target_fps */ }
    static void set_exit_key(int) { /* no-op: Metal/Sokol handles quit via sapp */ }
    static void set_trace_log_level(int) { /* no-op: uses afterhours logging */ }

    // ── Frame ──
    static void begin_drawing() {
        sg_pass pass{};
        pass.action = metal_detail::g_pass_action;
        pass.swapchain = sglue_swapchain();
        sg_begin_pass(&pass);

        // Set up sokol_gl orthographic projection for 2D drawing
        // Use LOGICAL window coordinates (not framebuffer pixels) so drawing
        // matches mouse coords and UI layout.  The Metal layer handles DPI scaling.
        float dpi = sapp_dpi_scale();
        float w = static_cast<float>(sapp_width()) / dpi;
        float h = static_cast<float>(sapp_height()) / dpi;
        sgl_defaults();
        sgl_matrix_mode_projection();
        sgl_ortho(0.0f, w, h, 0.0f, -1.0f, 1.0f);
    }

    static void end_drawing() {
        // Flush fontstash texture updates
        if (metal_detail::g_fons_ctx) {
            sfons_flush(metal_detail::g_fons_ctx);
        }
        // Render all sokol_gl draw calls
        sgl_draw();
        sg_end_pass();
        sg_commit();
    }

    static void clear_background(::afterhours::ColorLike auto c) {
        // Update pass action so sg_begin_pass clears to this color
        metal_detail::g_pass_action.colors[0].clear_value = {
            static_cast<float>(c.r) / 255.0f,
            static_cast<float>(c.g) / 255.0f,
            static_cast<float>(c.b) / 255.0f,
            static_cast<float>(c.a) / 255.0f,
        };
        // Draw a full-screen quad in logical coordinates so the first frame
        // isn't black (pass action only takes effect on the next begin_pass)
        float dpi = sapp_dpi_scale();
        float w = static_cast<float>(sapp_width()) / dpi;
        float h = static_cast<float>(sapp_height()) / dpi;
        sgl_begin_quads();
        sgl_c4b(c.r, c.g, c.b, c.a);
        sgl_v2f(0, 0);
        sgl_v2f(w, 0);
        sgl_v2f(w, h);
        sgl_v2f(0, h);
        sgl_end();
    }

    // ── Screen / timing ──
    static int get_screen_width() {
        float dpi = sapp_dpi_scale();
        return static_cast<int>(static_cast<float>(sapp_width()) / dpi);
    }
    static int get_screen_height() {
        float dpi = sapp_dpi_scale();
        return static_cast<int>(static_cast<float>(sapp_height()) / dpi);
    }
    static float get_frame_time() { return static_cast<float>(sapp_frame_duration()); }
    static float get_fps() {
        float dt = get_frame_time();
        return dt > 0.0f ? 1.0f / dt : 0.0f;
    }
    static double get_time() {
        return stm_sec(stm_since(metal_detail::g_start_time));
    }

    // ── Text measurement ──
    static int measure_text(const char* text, int font_size) {
        auto* ctx = metal_detail::g_fons_ctx;
        if (!ctx || metal_detail::g_active_font == FONS_INVALID) return 0;
        fonsSetFont(ctx, metal_detail::g_active_font);
        fonsSetSize(ctx, static_cast<float>(font_size));
        return static_cast<int>(fonsTextBounds(ctx, 0, 0, text, nullptr, nullptr));
    }

    // ── Screenshots ──
    // Implemented in sokol_impl.mm via CoreGraphics window capture
    static void take_screenshot(const char* filename);

    // ── Input ──
    static bool is_key_pressed_repeat(int key) {
        if (key <= 0 || key >= metal_detail::MAX_KEYS) return false;
        auto& s = metal_detail::input_state();
        return s.key_pressed[key] || s.key_repeat[key];
    }

    static bool is_key_pressed(int key) {
        if (key <= 0 || key >= metal_detail::MAX_KEYS) return false;
        return metal_detail::input_state().key_pressed[key];
    }

    static bool is_key_down(int key) {
        if (key <= 0 || key >= metal_detail::MAX_KEYS) return false;
        return metal_detail::input_state().key_down[key];
    }

    static bool is_key_released(int key) {
        if (key <= 0 || key >= metal_detail::MAX_KEYS) return false;
        return metal_detail::input_state().key_released[key];
    }

    static int get_char_pressed() {
        return static_cast<int>(metal_detail::pop_char());
    }

    static bool is_mouse_button_pressed(int btn) {
        if (btn < 0 || btn >= metal_detail::MAX_MOUSE_BUTTONS) return false;
        return metal_detail::input_state().mouse_pressed[btn];
    }

    static bool is_mouse_button_down(int btn) {
        if (btn < 0 || btn >= metal_detail::MAX_MOUSE_BUTTONS) return false;
        return metal_detail::input_state().mouse_down[btn];
    }

    static bool is_mouse_button_released(int btn) {
        if (btn < 0 || btn >= metal_detail::MAX_MOUSE_BUTTONS) return false;
        return metal_detail::input_state().mouse_released[btn];
    }

    static bool is_mouse_button_up(int btn) {
        return !is_mouse_button_down(btn);
    }

    struct Vec2 { float x, y; };

    static Vec2 get_mouse_position() {
        auto& s = metal_detail::input_state();
        return {s.mouse_x, s.mouse_y};
    }

    static Vec2 get_mouse_delta() {
        auto& s = metal_detail::input_state();
        return {s.mouse_dx, s.mouse_dy};
    }

    static float get_mouse_wheel_move() {
        return metal_detail::input_state().scroll_y;
    }

    static Vec2 get_mouse_wheel_move_v() {
        auto& s = metal_detail::input_state();
        return {s.scroll_x, s.scroll_y};
    }

    // ── Application control ──
    static void request_quit() { sapp_request_quit(); }

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

// ── Screenshot implementation (needs extern "C" outside class/namespace) ──
extern "C" void metal_take_screenshot(const char*);
inline void afterhours::graphics::MetalPlatformAPI::take_screenshot(const char* filename) {
    metal_take_screenshot(filename);
}

#endif  // AFTER_HOURS_USE_METAL
