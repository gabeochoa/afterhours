// sokol_msaa_test.cpp — antialiasing part A (decide-sokol-antialiasing).
// Headless Metal coverage test: a diagonal drawn into a 4x MSAA render
// target, resolved to 1x, must show partial-coverage (intermediate) edge
// pixels, while the 1x path shows a hard binary edge. Also proves the
// default-off path is unchanged: the headless target reports sample_count
// 1 with no resolve image, and its diagonal has no intermediate pixels.
//
// Own process: the Metal backend cannot be re-initialised. macOS/Metal only.

#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_VULKAN) && \
    !defined(AFTER_HOURS_USE_SOKOL)
#define AFTER_HOURS_USE_SOKOL
#endif

#include <afterhours/src/graphics.h>
#include <afterhours/src/backends/sokol/drawing_helpers.h>

#include <cstdio>
#include <string>
#include <vector>

namespace g = afterhours::graphics;
using afterhours::Color;

static int checks_run = 0, checks_passed = 0;
static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) checks_passed++;
  else fprintf(stderr, "  FAIL: %s\n", what.c_str());
}

static constexpr int W = 64, H = 64;

// Draw a white diagonal triangle covering roughly the lower-left half on a
// black background, read back RGBA.
static std::vector<uint8_t> render_diagonal(g::RenderTextureType &rt) {
  afterhours::begin_texture_mode(rt);
  afterhours::draw_rectangle(RectangleType{0, 0, W, H}, Color{0, 0, 0, 255});
  // Triangle (0,0)-(W,H)-(0,H): its hypotenuse is the diagonal edge.
  afterhours::draw_triangle(Vector2Type{0, 0}, Vector2Type{W, H},
                            Vector2Type{0, H}, Color{255, 255, 255, 255});
  afterhours::end_texture_mode();
  // Commit so the pass (and MSAA resolve at end_pass) completes before readback.
  sg_commit();
  auto cap = afterhours::capture_render_texture_rgba(rt);
  if (!cap) return {};
  return cap->pixels;
}

static int intermediate_count(const std::vector<uint8_t> &px) {
  int n = 0;
  for (size_t i = 0; i + 3 < px.size(); i += 4) {
    int r = px[i];
    if (r > 16 && r < 239) n++;
  }
  return n;
}

int main() {
  printf("=== sokol msaa tests ===\n\n");
  g::Config cfg;
  cfg.display = g::DisplayMode::Headless;
  cfg.width = W; cfg.height = H; cfg.title = "msaa test";
  // cfg.msaa_samples deliberately left at default 1 (off).
  if (!g::init(cfg)) {
    printf("SKIP: headless Metal init failed (no GPU available)\n");
    return 0;
  }

  auto &headless = g::metal_detail::g_headless_rt;
  check(headless.sample_count == 1, "default headless target is 1x (off)");
  check(headless.resolve_img_id == 0, "default headless target has no resolve image");

  auto plain = afterhours::load_render_texture(W, H);
  check(plain.color_img_id != 0 && plain.sample_count == 1,
        "plain render texture created at 1x");
  auto msaa = afterhours::load_msaa_render_texture(W, H, 4);
  check(msaa.color_img_id != 0, "msaa render texture created");
  check(msaa.sample_count == 4, "msaa render texture reports 4 samples");
  check(msaa.resolve_img_id != 0 && msaa.resolve_view_id != 0,
        "msaa render texture has a resolve image + view");
  check(afterhours::render_texture_capture_image_id(msaa) == msaa.resolve_img_id,
        "capture/readback points at the resolve image");
  check(afterhours::render_texture_capture_image_id(plain) == plain.color_img_id,
        "plain capture still points at the color image");

  auto plain_px = render_diagonal(plain);
  auto msaa_px = render_diagonal(msaa);
  check(plain_px.size() == (size_t)W * H * 4, "plain readback size");
  check(msaa_px.size() == (size_t)W * H * 4, "msaa readback size");

  const int plain_mid = intermediate_count(plain_px);
  const int msaa_mid = intermediate_count(msaa_px);
  printf("intermediate edge pixels: plain=%d msaa=%d\n", plain_mid, msaa_mid);
  check(plain_mid == 0, "1x diagonal has a hard edge (no intermediate pixels)");
  check(msaa_mid >= 16, "4x MSAA diagonal shows coverage (intermediate pixels)");

  // Default-off byte identity: rendering the same diagonal into the default
  // headless target must match the plain 1x target pixel for pixel.
  g::begin_drawing();
  afterhours::draw_rectangle(RectangleType{0, 0, W, H}, Color{0, 0, 0, 255});
  afterhours::draw_triangle(Vector2Type{0, 0}, Vector2Type{W, H},
                            Vector2Type{0, H}, Color{255, 255, 255, 255});
  g::end_drawing();
  auto headless_cap = afterhours::capture_render_texture_rgba(headless);
  check(headless_cap.has_value(), "headless default capture succeeds");
  if (headless_cap && plain_px.size() == headless_cap->pixels.size())
    check(headless_cap->pixels == plain_px,
          "default-off headless output is byte-identical to the 1x path");

  afterhours::unload_render_texture(msaa);
  afterhours::unload_render_texture(plain);
  g::shutdown();
  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) { printf("FAILURES: %d\n", checks_run - checks_passed); return 1; }
  printf("All checks passed!\n");
  return 0;
}
