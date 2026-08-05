// vendor/afterhours/src/graphics/metal_backend.h
// Metal/Sokol backend -- satisfies PlatformBackend concept.
//
// To use: #define AFTER_HOURS_USE_METAL (instead of AFTER_HOURS_USE_RAYLIB)
// Requires: sokol headers vendored in vendor/sokol/

#pragma once

#ifdef AFTER_HOURS_USE_METAL

#include "../../graphics_common.h"
#include "../../logging.h"

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
#include <sokol/sokol_log.h>
#include <sokol/sokol_time.h>

// 2D drawing (immediate-mode GL-style API on top of sokol_gfx)
#include <sokol/sokol_gl.h>

// Font rendering (fontstash + sokol integration)
#include <fontstash/fontstash.h>
#include <sokol/sokol_fontstash.h>
// Image decoding + the impl-TU sentinel (referenced from run() below so a
// missing SOKOL_IMPL translation unit produces a self-describing link error).
#include "image_decode.h"
#include <cmath>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>

// Shared state (also used by font_helper.h)
#include "metal_state.h"

// Offscreen render-texture helpers are defined later in
// backends/sokol/drawing_helpers.h — but that header pulls in graphics.h (hence
// this file) *before* it defines them, so forward-declare the few the headless
// frame path needs here. Any TU that odr-uses the headless branches also
// includes drawing_helpers.h (it renders UI), so the definitions are present.
namespace afterhours {
graphics::RenderTextureType load_render_texture(int w, int h);
void unload_render_texture(graphics::RenderTextureType &rt);
void begin_texture_mode(graphics::RenderTextureType &rt);
void end_texture_mode();
} // namespace afterhours

// Implemented in capture_impl.h (the SOKOL_IMPL Objective-C++ TU).
extern "C" const void *metal_create_system_device(void);
extern "C" bool metal_capture_render_texture(uint32_t color_img_id, int width,
                                             int height, const char *path);

namespace afterhours::graphics {

namespace metal_detail {

// ── Rendering pass action (local to metal_backend) ──
inline sg_pass_action g_pass_action{};
inline int g_camera_mode_depth = 0;
inline uint32_t g_next_shader_id = 1;
struct ShaderRecord {
  bool is_ps1_post = false;
  float resolution[2] = {0.0f, 0.0f};
};
inline std::unordered_map<uint32_t, ShaderRecord> g_shaders;
inline uint32_t g_active_shader_id = 0;

// Offscreen render target for headless (windowless) mode, created in metal_init.
inline RenderTextureType g_headless_rt{};

// src-over for 2D drawing; sgl_defaults() loads a pipeline with blending off,
// which discarded every alpha byte. Opaque draws are unchanged (a=255).
inline sgl_pipeline g_blend_pip{};

// sokol_gl + fontstash setup shared by the windowed (sokol_init_cb) and headless
// (metal_init) bootstraps. Assumes sg_setup() has already run.
inline void setup_sokol_gl_and_fonts() {
  sgl_desc_t sgl_desc{};
  sgl_desc.max_vertices = 1 << 18; // 262144 vertices
  sgl_desc.max_commands = 1 << 16; // 65536 commands
  sgl_desc.logger.func = slog_func;
  sgl_setup(&sgl_desc);

  sg_pipeline_desc blend_desc{};
  blend_desc.colors[0].blend.enabled = true;
  blend_desc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
  blend_desc.colors[0].blend.dst_factor_rgb =
      SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  blend_desc.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
  blend_desc.colors[0].blend.dst_factor_alpha =
      SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  g_blend_pip = sgl_make_pipeline(&blend_desc);

  sfons_desc_t sfons_desc{};
  sfons_desc.width = 2048;
  sfons_desc.height = 2048;
  g_fons_ctx = sfons_create(&sfons_desc);
  if (g_fons_ctx == nullptr) {
    log_error("sfons_create failed (2048x2048 atlas); text rendering disabled");
  }
}

// ── Input state ──
// Sokol keycodes are GLFW-compatible (0-511 range covers all keys).
static constexpr int MAX_KEYS = 512;
static constexpr int MAX_MOUSE_BUTTONS = 4;

// Per-key state: down tracks held, pressed/released are edge-triggered per
// frame.
struct InputState {
  bool key_down[MAX_KEYS]{};
  bool key_pressed[MAX_KEYS]{};  // went down this frame
  bool key_released[MAX_KEYS]{}; // went up this frame
  bool key_repeat[MAX_KEYS]{};   // repeat event this frame

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

inline InputState &input_state() {
  static InputState s{};
  return s;
}

// Call at the start of each frame to clear edge-triggered state.
inline void input_begin_frame() {
  auto &s = input_state();
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

// C0, DEL, and C1 -- control codes, not text. Kept here rather than shared with
// text_input so the backend does not depend on a UI plugin.
inline bool is_control_char_code(uint32_t c) {
  return (c < 32 && c != '\t') || c == 0x7F || (c >= 0x80 && c <= 0x9F);
}

inline void push_char(uint32_t c) {
  auto &s = input_state();
  int next = (s.char_queue_tail + 1) % InputState::CHAR_QUEUE_SIZE;
  if (next != s.char_queue_head) {
    s.char_queue[s.char_queue_tail] = c;
    s.char_queue_tail = next;
  }
}

inline uint32_t pop_char() {
  auto &s = input_state();
  if (s.char_queue_head == s.char_queue_tail)
    return 0;
  uint32_t c = s.char_queue[s.char_queue_head];
  s.char_queue_head = (s.char_queue_head + 1) % InputState::CHAR_QUEUE_SIZE;
  return c;
}

// ── Sokol callbacks ──

inline void sokol_init_cb() {
  sg_desc desc{};
  desc.environment = sglue_environment();
  desc.logger.func = slog_func;
  // sgl sample_count: the default sgl context here matches the swapchain (4x
  // MSAA); offscreen render textures are drawn through their own per-texture
  // sgl_context created with the render texture's sample_count (see
  // load_render_texture + begin_texture_mode in drawing_helpers.h), so each
  // pass draws with a pipeline whose sample_count matches its target. Validation
  // is enabled (no disable_validation).
  sg_setup(&desc);
  stm_setup();
  g_start_time = stm_now();

  // Initialize sokol_gl (2D drawing) + fontstash (text). Shared with headless.
  setup_sokol_gl_and_fonts();

  g_initialized = true;

  g_pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
  g_pass_action.colors[0].clear_value = {0.0f, 0.0f, 0.0f, 1.0f};

  if (g_init_fn)
    g_init_fn();
}

inline void sokol_frame_cb() {
  if (g_frame_fn)
    g_frame_fn();
  // Clear edge state after the frame callback has had a chance to read it.
  input_begin_frame();
}

inline void sokol_cleanup_cb() {
  if (g_cleanup_fn)
    g_cleanup_fn();
  if (g_fons_ctx) {
    sfons_destroy(g_fons_ctx);
    g_fons_ctx = nullptr;
  }
  sgl_shutdown();
  sg_shutdown();
  g_initialized = false;
}

inline void sokol_event_cb(const sapp_event *ev) {
  auto &s = input_state();
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
    // macOS sends DEL (0x7F) for Backspace as a CHAR event; queueing it makes
    // backspace type a blank. Only real text belongs in the char queue.
    if (ev->char_code > 0 && !is_control_char_code(ev->char_code)) {
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
} // namespace metal_detail

struct MetalPlatformAPI {
  // Lightweight color struct — satisfies ColorLike without pulling in color.h
  struct color_type {
    unsigned char r, g, b, a;
  };

  // ── Constants ──
  static constexpr unsigned int FLAG_WINDOW_RESIZABLE = 0x00000004;
  static constexpr unsigned int FLAG_VSYNC_HINT = 0x00000040;
  static constexpr int LOG_ERROR = 5;
  static constexpr int TEXTURE_FILTER_BILINEAR = 1;
  static constexpr int TEXTURE_FILTER_POINT = 0;
  static constexpr int SHADER_UNIFORM_VEC2 = 1;

  // ── Window lifecycle (legacy API -- prefer run()) ──
  static void init_window(int, int, const char *) {
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
  // Runtime window sizing controls are intentionally unsupported in the
  // sokol_app layer. Sokol exposes startup size via sapp_desc, but does not
  // provide a portable runtime API for set_window_size / set_window_min_size.
  // Keep these loud so callers know this is a backend limitation, not a bug.
  static void minimize_window() {
    log_error("@notimplemented minimize_window");
  }
  static void set_window_size(int w, int h) {
    // Headless: there is no window, but we can honor a resize by re-sizing the
    // offscreen render target + reported screen dims, so layout/e2e that depend
    // on a specific viewport size work windowlessly. Called on resize events
    // only (not per-frame), so recreating the render texture here is fine.
    if (metal_detail::g_headless) {
      if (w <= 0 || h <= 0)
        return;
      if (w == metal_detail::g_headless_w && h == metal_detail::g_headless_h)
        return;
      metal_detail::g_headless_w = w;
      metal_detail::g_headless_h = h;
      ::afterhours::unload_render_texture(metal_detail::g_headless_rt);
      metal_detail::g_headless_rt = ::afterhours::load_render_texture(w, h);
      return;
    }
    log_error("@notimplemented set_window_size");
  }
  static void set_window_min_size(int, int) {
    log_error("@notimplemented set_window_min_size");
  }
  static void set_window_state(unsigned int flags) {
    // afterhours keeps raylib-style window-state flags in a backend-neutral
    // API. Sokol has specific APIs for selected features (e.g. fullscreen),
    // so we track requested bits here for parity and future mapping.
    metal_detail::g_window_state_flags |= flags;
  }
  static void clear_window_state(unsigned int flags) {
    metal_detail::g_window_state_flags &= ~flags;
  }

  // ── Config (legacy API -- prefer RunConfig fields) ──
  static void set_config_flags(unsigned int) { /* handled via RunConfig.flags */
  }
  static void set_target_fps(int) { /* handled via RunConfig.target_fps */ }
  static void set_exit_key(int) { /* no-op: Metal/Sokol handles quit via sapp */
  }
  static void set_trace_log_level(int) { /* no-op: uses afterhours logging */ }

  // ── Frame ──
  static void begin_drawing() {
    // Headless: no swapchain — render into the offscreen texture instead. This
    // sets up the ortho projection + GL→Metal fixup internally, so we return.
    if (metal_detail::g_headless) {
      ::afterhours::begin_texture_mode(metal_detail::g_headless_rt);
      return;
    }

    sg_pass pass{};
    pass.action = metal_detail::g_pass_action;
    pass.swapchain = sglue_swapchain();
    sg_begin_pass(&pass);
    metal_detail::g_pass_active = true;

    // Set up sokol_gl orthographic projection for 2D drawing.
    // With high_dpi=true the framebuffer may be larger than the logical
    // window (e.g. 2x on Retina).  We project in logical (CSS) pixels so
    // UI code doesn't need to know about DPI; the GPU rasterises into the
    // full-res framebuffer automatically.
    float dpi = sapp_dpi_scale();
    float w = static_cast<float>(sapp_width()) / dpi;
    float h = static_cast<float>(sapp_height()) / dpi;
    sgl_defaults();
    sgl_load_pipeline(metal_detail::g_blend_pip);
    sgl_matrix_mode_projection();
    // sokol_gl produces OpenGL clip-space Z [-1,+1]; Metal clips to [0,+1].
    // Pre-load a fixup so ortho output lands in Metal's depth range.
    // clang-format off
    static const float gl_to_metal[16] = {
        1,0,0,0,  0,1,0,0,  0,0,0.5f,0,  0,0,0.5f,1
    };
    // clang-format on
    sgl_load_matrix(gl_to_metal);
    sgl_ortho(0.0f, w, h, 0.0f, -1.0f, 1.0f);
    sgl_matrix_mode_modelview();
    sgl_load_identity();
  }

  static void end_drawing() {
    // Headless: close the offscreen pass (flushes fontstash + sokol_gl draws)
    // and submit the GPU work so the render texture is ready for readback.
    if (metal_detail::g_headless) {
      ::afterhours::end_texture_mode();
      sg_commit();
      return;
    }

    if (metal_detail::g_camera_mode_depth != 0) {
      while (metal_detail::g_camera_mode_depth > 0) {
        sgl_matrix_mode_modelview();
        sgl_pop_matrix();
        --metal_detail::g_camera_mode_depth;
      }
      log_warn("Unbalanced begin_mode_2d/end_mode_2d; camera stack reset at frame end");
    }
    // Flush fontstash texture updates
    if (metal_detail::g_fons_ctx) {
      sfons_flush(metal_detail::g_fons_ctx);
    }
    // Render all sokol_gl draw calls
    sgl_draw();
    sg_end_pass();
    metal_detail::g_pass_active = false;
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
    // Draw a full-screen quad so the first frame isn't black
    // (pass action only takes effect on the next begin_pass). Headless has no
    // swapchain, so size the quad from the render texture via the shim.
    float dpi_bg = metal_detail::dpi_scale();
    float w = static_cast<float>(metal_detail::screen_w()) / dpi_bg;
    float h = static_cast<float>(metal_detail::screen_h()) / dpi_bg;
    sgl_begin_quads();
    sgl_c4b(c.r, c.g, c.b, c.a);
    sgl_v2f(0, 0);
    sgl_v2f(w, 0);
    sgl_v2f(w, h);
    sgl_v2f(0, h);
    sgl_end();
  }

  // ── Screen / timing ──
  // Return logical (CSS) pixel dimensions, not framebuffer pixels.
  static int get_screen_width() {
    return static_cast<int>(static_cast<float>(metal_detail::screen_w()) /
                            metal_detail::dpi_scale());
  }
  static int get_screen_height() {
    return static_cast<int>(static_cast<float>(metal_detail::screen_h()) /
                            metal_detail::dpi_scale());
  }
  static float get_frame_time() {
    // No sokol_app in headless: sapp_frame_duration() is invalid. Advance the
    // sim at a fixed step so frame-driven logic still progresses.
    if (metal_detail::g_headless)
      return 1.0f / 60.0f;
    return static_cast<float>(sapp_frame_duration());
  }
  static float get_fps() {
    float dt = get_frame_time();
    return dt > 0.0f ? 1.0f / dt : 0.0f;
  }
  static double get_time() {
    return stm_sec(stm_since(metal_detail::g_start_time));
  }

  // ── Text measurement ──
  static int measure_text(const char *text, int font_size) {
    auto *ctx = metal_detail::g_fons_ctx;
    if (!ctx || metal_detail::g_active_font == FONS_INVALID)
      return 0;
    fonsSetFont(ctx, metal_detail::g_active_font);
    float dpi = metal_detail::dpi_scale();
    fonsSetSize(ctx, static_cast<float>(font_size) * dpi);
    return static_cast<int>(fonsTextBounds(ctx, 0, 0, text, nullptr, nullptr) / dpi);
  }

  // ── Screenshots ──
  // Implemented in sokol_impl.mm via CoreGraphics window capture
  static void take_screenshot(const char *filename);
  static void set_render_texture_filter(RenderTextureType &rt, int filter) {
    const bool point = (filter == TEXTURE_FILTER_POINT);
    if (rt.sampler_id) {
      sg_destroy_sampler({rt.sampler_id});
      rt.sampler_id = 0;
    }
    sg_sampler_desc sd{};
    sd.min_filter = point ? SG_FILTER_NEAREST : SG_FILTER_LINEAR;
    sd.mag_filter = point ? SG_FILTER_NEAREST : SG_FILTER_LINEAR;
    sd.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    sd.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    sd.label = point ? "rt-sampler-point" : "rt-sampler-linear";
    rt.sampler_id = sg_make_sampler(&sd).id;
  }

  // ── Shaders ──
  static ShaderType load_shader(const char *, const char *fsFileName) {
    ShaderType shader{};
    if (fsFileName == nullptr) {
      log_error("load_shader: fragment shader path is null");
      return shader;
    }
    if (!std::filesystem::exists(fsFileName)) {
      log_error("load_shader: file not found '{}'", fsFileName);
      return shader;
    }

    const uint32_t id = metal_detail::g_next_shader_id++;
    metal_detail::ShaderRecord rec{};
    rec.is_ps1_post = std::string(fsFileName).find("ps1_post.fs") != std::string::npos;
    metal_detail::g_shaders[id] = rec;
    shader.id = id;
    return shader;
  }
  static void unload_shader(ShaderType &shader) {
    if (shader.id == 0)
      return;
    metal_detail::g_shaders.erase(shader.id);
    if (metal_detail::g_active_shader_id == shader.id) {
      metal_detail::g_active_shader_id = 0;
      metal_detail::g_shader_runtime = {};
    }
    shader.id = 0;
  }
  static int get_shader_location(ShaderType &shader, const char *uniformName) {
    if (shader.id == 0 || uniformName == nullptr)
      return -1;
    if (std::strcmp(uniformName, "resolution") == 0)
      return 1;
    return -1;
  }
  static void set_shader_value(ShaderType &shader, int locIndex, const void *value,
                               int uniformType) {
    if (shader.id == 0 || value == nullptr)
      return;
    auto it = metal_detail::g_shaders.find(shader.id);
    if (it == metal_detail::g_shaders.end())
      return;
    if (locIndex == 1 && uniformType == SHADER_UNIFORM_VEC2) {
      const float *vec2 = static_cast<const float *>(value);
      it->second.resolution[0] = vec2[0];
      it->second.resolution[1] = vec2[1];
    }
  }
  static void begin_shader_mode(ShaderType &shader) {
    if (shader.id == 0)
      return;
    auto it = metal_detail::g_shaders.find(shader.id);
    if (it == metal_detail::g_shaders.end())
      return;
    metal_detail::g_active_shader_id = shader.id;
    metal_detail::g_shader_runtime.ps1_enabled = it->second.is_ps1_post;
    metal_detail::g_shader_runtime.resolution[0] = it->second.resolution[0];
    metal_detail::g_shader_runtime.resolution[1] = it->second.resolution[1];
  }
  static void end_shader_mode() {
    metal_detail::g_active_shader_id = 0;
    metal_detail::g_shader_runtime = {};
  }

  // ── Input ──
  static bool is_key_pressed_repeat(int key) {
    if (key <= 0 || key >= metal_detail::MAX_KEYS)
      return false;
    auto &s = metal_detail::input_state();
    return s.key_pressed[key] || s.key_repeat[key];
  }

  static bool is_key_pressed(int key) {
    if (key <= 0 || key >= metal_detail::MAX_KEYS)
      return false;
    return metal_detail::input_state().key_pressed[key];
  }

  static bool is_key_down(int key) {
    if (key <= 0 || key >= metal_detail::MAX_KEYS)
      return false;
    return metal_detail::input_state().key_down[key];
  }

  static bool is_key_released(int key) {
    if (key <= 0 || key >= metal_detail::MAX_KEYS)
      return false;
    return metal_detail::input_state().key_released[key];
  }

  static int get_char_pressed() {
    return static_cast<int>(metal_detail::pop_char());
  }

  static bool is_mouse_button_pressed(int btn) {
    if (btn < 0 || btn >= metal_detail::MAX_MOUSE_BUTTONS)
      return false;
    return metal_detail::input_state().mouse_pressed[btn];
  }

  static bool is_mouse_button_down(int btn) {
    if (btn < 0 || btn >= metal_detail::MAX_MOUSE_BUTTONS)
      return false;
    return metal_detail::input_state().mouse_down[btn];
  }

  static bool is_mouse_button_released(int btn) {
    if (btn < 0 || btn >= metal_detail::MAX_MOUSE_BUTTONS)
      return false;
    return metal_detail::input_state().mouse_released[btn];
  }

  static bool is_mouse_button_up(int btn) { return !is_mouse_button_down(btn); }

  struct Vec2 {
    float x, y;
  };
  struct Camera2D {
    Vec2 offset;
    Vec2 target;
    float rotation;
    float zoom;
  };

  static Vec2 get_mouse_position() {
    auto &s = metal_detail::input_state();
    // sokol reports mouse coords in framebuffer pixels (scaled by DPI on
    // macOS Retina), but all UI layout/rendering uses logical pixels.
    float dpi = sapp_dpi_scale();
    return {s.mouse_x / dpi, s.mouse_y / dpi};
  }
  static Vec2 get_screen_to_world_2d(const Vec2 &position,
                                     const Camera2D &camera) {
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
    const float zoom = (camera.zoom == 0.0f) ? 1.0f : camera.zoom;
    const float x = (position.x - camera.offset.x) / zoom;
    const float y = (position.y - camera.offset.y) / zoom;

    const float rot = -camera.rotation * kDegToRad;
    const float c = std::cos(rot);
    const float s = std::sin(rot);

    return {x * c - y * s + camera.target.x, x * s + y * c + camera.target.y};
  }
  static Vec2 get_world_to_screen_2d(const Vec2 &position,
                                     const Camera2D &camera) {
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
    const float zoom = (camera.zoom == 0.0f) ? 1.0f : camera.zoom;
    const float x = position.x - camera.target.x;
    const float y = position.y - camera.target.y;

    const float rot = camera.rotation * kDegToRad;
    const float c = std::cos(rot);
    const float s = std::sin(rot);

    return {camera.offset.x + (x * c - y * s) * zoom,
            camera.offset.y + (x * s + y * c) * zoom};
  }
  static void begin_mode_2d(const Camera2D &camera) {
    if (!metal_detail::g_pass_active) {
      log_warn("begin_mode_2d called outside active drawing pass");
      return;
    }
    sgl_matrix_mode_modelview();
    sgl_push_matrix();
    ++metal_detail::g_camera_mode_depth;

    // Match raylib Camera2D transform:
    // screen = T(offset) * R(rotation) * S(zoom) * T(-target) * world
    sgl_translate(camera.offset.x, camera.offset.y, 0.0f);
    sgl_rotate(sgl_rad(camera.rotation), 0.0f, 0.0f, 1.0f);
    sgl_scale(camera.zoom, camera.zoom, 1.0f);
    sgl_translate(-camera.target.x, -camera.target.y, 0.0f);
  }
  static void end_mode_2d() {
    if (metal_detail::g_camera_mode_depth <= 0) {
      log_warn("end_mode_2d called without matching begin_mode_2d");
      return;
    }
    sgl_matrix_mode_modelview();
    sgl_pop_matrix();
    --metal_detail::g_camera_mode_depth;
  }

  static Vec2 get_mouse_delta() {
    auto &s = metal_detail::input_state();
    float dpi = sapp_dpi_scale();
    return {s.mouse_dx / dpi, s.mouse_dy / dpi};
  }

  static float get_mouse_wheel_move() {
    return metal_detail::input_state().scroll_y;
  }

  static Vec2 get_mouse_wheel_move_v() {
    auto &s = metal_detail::input_state();
    return {s.scroll_x, s.scroll_y};
  }

  // ── Application control ──
  static void request_quit() { sapp_request_quit(); }

  // ── Unified run loop ──
  static void run(const RunConfig &cfg) {
    // Force a link-time reference to the impl-TU sentinel. If the project
    // forgot to add the one SOKOL_IMPL Objective-C++ TU, the linker fails here
    // on a symbol whose name spells out the fix (see image_decode.h). Two twins
    // are referenced: the macOS-worded one and a platform-neutral one, so the
    // error reads correctly on Metal, GL (Linux), D3D11 (Windows), and web.
    volatile int _ah_sokol_impl_present =
        AFTERHOURS_MISSING_sokol_impl_TU__add_one_objcpp_file_that_defines_SOKOL_IMPL_and_includes_afterhours_src_backends_sokol_image_decode_h() +
        AFTERHOURS_MISSING_sokol_impl_TU__add_one_TU_that_defines_SOKOL_IMPL_and_includes_afterhours_src_backends_sokol_image_decode_h();
    (void)_ah_sokol_impl_present;

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
    // Enable high-DPI rendering: on Retina displays the framebuffer is at
    // native resolution (e.g. 2x) for crisp text and UI.  All UI code
    // works in logical (CSS) pixels; the ortho projection and input layer
    // handle the DPI conversion transparently.
    desc.high_dpi = true;
    desc.sample_count = 1;
    desc.enable_clipboard = true;
    desc.clipboard_size = 16 * 1024;  // 16 KB clipboard buffer

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

namespace metal_backend {

inline bool metal_init(const Config &cfg) {
  if (cfg.display == DisplayMode::Headless) {
    // Windowless offscreen rendering: create our own Metal device (no
    // sokol_app / WindowServer), set up sokol_gfx against it with no swapchain,
    // and render into an offscreen texture we can read back to PNG.
    metal_detail::g_headless = true;
    metal_detail::g_headless_w = cfg.width;
    metal_detail::g_headless_h = cfg.height;

    const void *device = metal_create_system_device();
    if (!device) {
      log_error("metal headless: MTLCreateSystemDefaultDevice failed (no GPU?)");
      metal_detail::g_headless = false;
      return false;
    }

    sg_desc desc{};
    desc.environment.metal.device = device;
    // No swapchain to query, so give load_render_texture sane defaults.
    desc.environment.defaults.color_format = SG_PIXELFORMAT_BGRA8;
    desc.environment.defaults.depth_format = SG_PIXELFORMAT_DEPTH_STENCIL;
    desc.environment.defaults.sample_count = 1;
    desc.logger.func = slog_func;
    sg_setup(&desc);
    if (!sg_isvalid()) {
      log_error("metal headless: sg_setup failed");
      metal_detail::g_headless = false;
      return false;
    }
    stm_setup();
    metal_detail::g_start_time = stm_now();
    metal_detail::setup_sokol_gl_and_fonts();
    metal_detail::g_initialized = true;

    const int scale =
        cfg.hidpi ? (cfg.hidpi_scale > 1 ? cfg.hidpi_scale : 2) : 1;
    graphics::render_scale() = scale;
    metal_detail::g_headless_rt =
        load_render_texture(cfg.width * scale, cfg.height * scale);
    metal_detail::g_headless_rt.scale = scale;
    if (metal_detail::g_headless_rt.color_img_id == 0) {
      log_error("metal headless: offscreen render texture creation failed "
                "({}x{})",
                cfg.width, cfg.height);
      return false;
    }
    return true;
  }
  // Sokol owns the app lifecycle via sapp_run() in MetalPlatformAPI::run().
  // Keep the windowed graphics::init() path unsupported (use graphics::run).
  log_error("@notimplemented graphics::init on metal backend; use graphics::run");
  return false;
}

inline void metal_shutdown() {
  if (!metal_detail::g_headless)
    return;
  unload_render_texture(metal_detail::g_headless_rt);
  if (metal_detail::g_fons_ctx) {
    sfons_destroy(metal_detail::g_fons_ctx);
    metal_detail::g_fons_ctx = nullptr;
  }
  sgl_shutdown();
  sg_shutdown();
  metal_detail::g_initialized = false;
  metal_detail::g_headless = false;
}

inline void metal_begin_frame() { MetalPlatformAPI::begin_drawing(); }

inline void metal_end_frame() { MetalPlatformAPI::end_drawing(); }

inline bool metal_capture_frame(const std::filesystem::path &path) {
  if (!metal_detail::g_headless)
    return false;
  auto &rt = metal_detail::g_headless_rt;
  if (rt.color_img_id == 0)
    return false;
  return metal_capture_render_texture(rt.color_img_id, rt.width, rt.height,
                                      path.string().c_str());
}

inline float metal_get_delta_time() {
  // No sokol_app in headless: sapp_frame_duration() is invalid, so advance the
  // sim at a fixed step.
  if (metal_detail::g_headless)
    return 1.0f / 60.0f;
  return MetalPlatformAPI::get_frame_time();
}

inline bool metal_is_headless() { return metal_detail::g_headless; }

inline RenderTextureType &metal_get_render_texture() {
  return metal_detail::g_headless_rt;
}

inline void ensure_registered() {
  static bool registered = false;
  if (!registered) {
    BackendInterface iface{};
    iface.init = metal_init;
    iface.shutdown = metal_shutdown;
    iface.begin_frame = metal_begin_frame;
    iface.end_frame = metal_end_frame;
    iface.capture_frame = metal_capture_frame;
    iface.get_delta_time = metal_get_delta_time;
    iface.is_headless = metal_is_headless;
    iface.get_render_texture = metal_get_render_texture;
    register_backend(iface);
    registered = true;
  }
}

namespace detail {
struct AutoRegister {
  AutoRegister() { ensure_registered(); }
};
inline AutoRegister auto_register{};
} // namespace detail

} // namespace metal_backend

} // namespace afterhours::graphics

extern "C" void metal_take_screenshot(const char *);
inline void
afterhours::graphics::MetalPlatformAPI::take_screenshot(const char *filename) {
  metal_take_screenshot(filename);
}

#endif // AFTER_HOURS_USE_METAL
