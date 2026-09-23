// tools/headless_capture/headless_capture.mm
//
// Windowless render-to-PNG harness for the afterhours sokol/Metal backend.
//
// This is "something like wordproc that outputs to a file for headless": it
// sets up sokol_gfx on Metal WITHOUT sokol_app (no window, no swapchain, no
// drawable, no frame loop), renders a test scene into an OFFSCREEN render
// texture, and writes the result to a PNG via the backend's existing
// CoreGraphics/ImageIO capture path (capture_impl.h).
//
// Because it never touches a swapchain drawable, it runs headless / lid-closed.
//
// Build + run (see Makefile in this directory):
//   make
//   ./headless_capture out.png
//
// The binary must be force-adhoc-signed to execute on this machine:
//   codesign -s - -f headless_capture
// (the Makefile does this automatically).

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#define AFTER_HOURS_USE_METAL // selects the sokol backend

#include <afterhours/ah.h>
#include <afterhours/src/drawing_helpers.h>
#include <afterhours/src/graphics.h>
#include <afterhours/src/plugins/effects.h>
#include <afterhours/src/plugins/files.h>

extern "C" int metal_capture_render_texture_to_memory(uint32_t color_img_id,
                                                      int width, int height,
                                                      uint8_t **out_data,
                                                      int *out_size);

// Metal device creation for the headless sg_environment.
#import <Metal/Metal.h>

using namespace afterhours;
namespace g = afterhours::graphics;

static constexpr int RT_W = 800;
static constexpr int RT_H = 600;
static constexpr int CHECKER_DIM = 64; // 8x8 cells of 8px each

// Build a procedural RGBA checker texture (no file IO -> AMFI-safe, no stb
// needed at runtime), matching the web demo's showcase texture.
static TextureType make_checker_texture() {
  std::vector<unsigned char> px(CHECKER_DIM * CHECKER_DIM * 4);
  for (int y = 0; y < CHECKER_DIM; ++y) {
    for (int x = 0; x < CHECKER_DIM; ++x) {
      const bool on = ((x / 8) + (y / 8)) % 2 == 0;
      unsigned char *p = px.data() + (y * CHECKER_DIM + x) * 4;
      if (on) {
        p[0] = 240; p[1] = 200; p[2] = 60;  p[3] = 255; // amber
      } else {
        p[0] = 40;  p[1] = 60;  p[2] = 120; p[3] = 255; // slate blue
      }
    }
  }
  return metal_texture_detail::load_texture_from_pixels(
      px.data(), CHECKER_DIM, CHECKER_DIM, TEXTURE_FILTER_BILINEAR);
}

// begin_texture_mode() uses SG_LOADACTION_LOAD for color (it assumes a prior
// clear from begin_drawing on the swapchain). Headless, the RT color image
// starts undefined, so do one explicit clear pass on the RT first.
static void clear_render_texture(const g::RenderTextureType &rt, Color c) {
  sg_pass pass{};
  pass.action.colors[0].load_action = SG_LOADACTION_CLEAR;
  pass.action.colors[0].clear_value = {c.r / 255.f, c.g / 255.f, c.b / 255.f,
                                       c.a / 255.f};
  pass.action.depth.load_action = SG_LOADACTION_CLEAR;
  pass.action.depth.clear_value = 1.0f;
  pass.attachments.colors[0] = {rt.color_view_id};
  pass.attachments.depth_stencil = {rt.depth_view_id};
  sg_begin_pass(&pass);
  sg_end_pass();
}

static int run_blur_test(const char *out_path, const char *resource_dir) {
  afterhours::files::init("headless_capture", resource_dir);
  constexpr int W = 800, H = 600;
  g::RenderTextureType rt = load_render_texture(W, H);
  clear_render_texture(rt, Color{0, 0, 0, 255});
  TextureType checker = make_checker_texture();
  begin_texture_mode(rt);
  draw_texture_rec(checker, RectangleType{0, 0, checker.width, checker.height},
                   Vector2Type{700, 500}, Color{255, 255, 255, 255});
  draw_rectangle(RectangleType{0, 0, W / 2.f, static_cast<float>(H)},
                 Color{255, 255, 255, 255});
  draw_rectangle(RectangleType{0, 0, 50, 50}, Color{0, 0, 0, 255});
  end_texture_mode();
  // checker kept alive until process exit (blur test)

  afterhours::effects::BlurPass pass;
  if (!pass.effect.ok()) {
    std::fprintf(stderr, "blur test: blur shader did not load\n");
    return 1;
  }
  begin_texture_mode(rt);
  pass.apply(rt, RectangleType{0, 0, static_cast<float>(W), static_cast<float>(H)}, 8.f);
  end_texture_mode();
  sg_commit();

  uint8_t *px = nullptr;
  int px_size = 0;
  if (!metal_capture_render_texture_to_memory(rt.color_img_id, W, H, &px, &px_size)) {
    std::fprintf(stderr, "blur test: readback failed\n");
    return 1;
  }
  const auto sample = [&](int x, int y) {
    return static_cast<int>(px[(static_cast<size_t>(y) * W + x) * 4]);
  };
  const int left = sample(60, H / 2), right = sample(W - 60, H / 2);
  const int edge_in = sample(W / 2, H / 2), edge_out = sample(W / 2 + 2, H / 2);
  const int corner_top = sample(25, 25), corner_bottom = sample(25, H - 25);
  std::printf("blur test: left=%d right=%d edge_in=%d edge_out=%d corner_top=%d "
              "corner_bottom=%d\n",
              left, right, edge_in, edge_out, corner_top, corner_bottom);
  capture_render_texture(rt, out_path);
  const bool ok = left > 200 && right < 55 && edge_in > 30 && edge_in < 225 &&
                  edge_out > 30 && edge_out < 225 && corner_top < 55 &&
                  corner_bottom > 200;
  std::printf("blur test: %s\n", ok ? "PASS" : "FAIL");
  free(px);
  unload_render_texture(rt);
  return ok ? 0 : 1;
}

int main(int argc, char **argv) {
  const char *out_path = (argc > 1) ? argv[1] : "out.png";
  const bool blur_mode = argc > 2 && std::strcmp(argv[2], "blur") == 0;

  // ---- Headless sokol setup: Metal device, NO swapchain / window ----
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  if (!device) {
    std::fprintf(stderr, "MTLCreateSystemDefaultDevice failed\n");
    return 1;
  }

  sg_desc desc{};
  desc.logger.func = slog_func;
  desc.environment.metal.device = (__bridge const void *)device;
  // No swapchain defaults are provided; render textures below pick sane
  // formats (BGRA8 color, DEPTH_STENCIL depth, 1x sample) when the swapchain
  // defaults are _SG_PIXELFORMAT_DEFAULT.
  sg_setup(&desc);
  if (!sg_isvalid()) {
    std::fprintf(stderr, "sg_setup failed / context invalid\n");
    return 1;
  }

  // The backend bootstrap (blend pipelines + fontstash), not a bare
  // sgl_setup: begin_texture_mode loads g_blend_pip, and drawing into a
  // pass whose pipeline handle is invalid aborts at flush time.
  afterhours::graphics::metal_detail::setup_sokol_gl_and_fonts();

  if (blur_mode)
    return run_blur_test(out_path, argc > 3 ? argv[3] : "../../../resources");

  // ---- Offscreen render target ----
  g::RenderTextureType rt = load_render_texture(RT_W, RT_H);

  TextureType checker = make_checker_texture();

  // ---- Render the test scene into the RT ----
  clear_render_texture(rt, Color{18, 18, 26, 255}); // dark background

  begin_texture_mode(rt);
  {
    const Color white{255, 255, 255, 255};
    const float tw = checker.width;
    const float th = checker.height;
    const RectangleType full{0, 0, tw, th};

    // 1) draw_texture_rec: 1:1 blit, upper-left area.
    draw_texture_rec(checker, full, Vector2Type{40, 40}, white);

    // 2) draw_texture_pro: 3x scaled, rotated 30 degrees about its center.
    const float scale = 3.0f;
    const RectangleType dest{620, 160, tw * scale, th * scale};
    const Vector2Type origin{tw * scale / 2.f, th * scale / 2.f};
    draw_texture_pro(checker, full, dest, origin, 30.0f, white);

    // 3) new geometry primitives -------------------------------------------
    // ring (donut)
    draw_ring(180, 420, 40, 80, 48, Color{80, 220, 160, 255});
    // regular polygon (hexagon)
    draw_poly(Vector2Type{380, 420}, 6, 70, 0.0f, Color{240, 120, 80, 255});
    // ellipse
    draw_ellipse(560, 420, 90, 55, Color{120, 160, 250, 255});
    // circle sector (pie wedge, 60 -> 300 degrees)
    draw_circle_sector(Vector2Type{720, 440}, 70, 60.0f, 300.0f, 48,
                       Color{250, 210, 90, 255});

    // A couple filled rects as reference registration marks.
    draw_rectangle(RectangleType{40, 540, 40, 40}, Color{255, 0, 0, 255});
    draw_rectangle(RectangleType{720, 540, 40, 40}, Color{0, 255, 0, 255});
  }
  end_texture_mode();

  // Commit the frame so the GPU work is submitted before readback.
  sg_commit();

  // ---- Capture to PNG (CoreGraphics/ImageIO, no window needed) ----
  bool ok = capture_render_texture(rt, out_path);
  if (!ok) {
    std::fprintf(stderr, "capture_render_texture failed\n");
    return 1;
  }
  std::printf("wrote %s (%dx%d)\n", out_path, RT_W, RT_H);

  unload_render_texture(rt);
  // checker kept alive until process exit (blur test)
  sgl_shutdown();
  sg_shutdown();
  return 0;
}
