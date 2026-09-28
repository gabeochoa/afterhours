// sokol_geometry_test.cpp
// Pixel-level proofs for the hanabi rendering-nits batch (todo.md item 14).
// Each block names its nit and asserts what the GPU actually produced, in
// the style of sokol_blend_test: headless Metal, offscreen readback.
//
//   #78 draw_circle_v truncated its centre to int (sokol only; raylib
//       passes the float centre through).
//   #25 a rounded rect with mixed round/sharp corners drew a diagonal
//       slice across the sharp corners.
//   #80/#110 boxes rasterize 1px bigger / shifted up-left: probed with a
//       fractional-origin rect whose exact pixel coverage is known.
//
// macOS/Metal only -- registered in tests/Makefile behind UNAME_S == Darwin.

#define AFTER_HOURS_USE_METAL

#include <afterhours/src/graphics.h>
#include <afterhours/src/backends/sokol/drawing_helpers.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace g = afterhours::graphics;
using afterhours::Color;

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s\n", what.c_str());
  }
}

static constexpr int W = 200;
static constexpr int H = 200;
static const Color BLACK{0, 0, 0, 255};
static const Color WHITE{255, 255, 255, 255};

// Draw one frame: black backdrop, then the caller's shapes; return pixels.
template <typename Draw> static std::vector<uint8_t> render(Draw draw) {
  g::begin_drawing();
  afterhours::draw_rectangle(RectangleType{0, 0, W, H}, BLACK);
  afterhours::draw_rectangle(RectangleType{0, 0, W, H}, BLACK);
  draw();
  g::end_drawing();
  auto shot = afterhours::capture_render_texture_rgba(
      g::metal_detail::g_headless_rt);
  if (!shot) return {};
  return shot->pixels;
}

static bool lit(const std::vector<uint8_t> &px, int x, int y) {
  const size_t i = (static_cast<size_t>(y) * W + x) * 4;
  return px.size() == static_cast<size_t>(W) * H * 4 && px[i] > 127;
}

int main() {
  printf("=== sokol geometry tests ===\n\n");

  g::Config cfg;
  cfg.display = g::DisplayMode::Headless;
  cfg.width = W;
  cfg.height = H;
  cfg.title = "geometry test";
  if (!g::init(cfg)) {
    printf("SKIP: headless Metal init failed (no GPU available)\n");
    return 0;
  }

  // #78: a circle centred at (20.9, 20.9) must be centred there, not at
  // (20, 20). Judged by the centroid of the filled pixels, in pixel-centre
  // coordinates, which is robust to rasterization edge rules.
  {
    auto px = render([] {
      afterhours::draw_circle_v(Vector2Type{20.9f, 20.9f}, 6.f, WHITE);
    });
    double sx = 0, sy = 0;
    long n = 0;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++)
        if (lit(px, x, y)) { sx += x + 0.5; sy += y + 0.5; n++; }
    check(n > 50, "#78 circle rasterized");
    if (n > 0) {
      const double cx = sx / n, cy = sy / n;
      printf("  #78 centroid: (%.2f, %.2f) for centre (20.9, 20.9)\n", cx, cy);
      check(std::abs(cx - 20.9) < 0.35 && std::abs(cy - 20.9) < 0.35,
            "#78 circle centroid sits at the fractional centre");
    }
  }

  // #25: only the top-left corner rounded (bit 3). The sharp corners must
  // stay square and the rounded corner must actually curve.
  {
    auto px = render([] {
      std::bitset<4> corners;
      corners.set(3);
      afterhours::draw_rectangle_rounded(
          RectangleType{40, 40, 80, 60}, 1.0f, 8, WHITE, corners);
    });
    check(!lit(px, 40, 40), "#25 rounded TL corner cuts its pixel");
    check(lit(px, 49, 49), "#25 TL arc interior is filled");
    check(lit(px, 119, 40), "#25 sharp TR corner stays square");
    check(lit(px, 119, 99), "#25 sharp BR corner stays square");
    check(lit(px, 40, 99), "#25 sharp BL corner stays square");
  }

  // #80/#110: a rect at (10.4, 10.4) sized 20.3x20.3 covers exactly the
  // pixels whose centres fall inside it: 10..30 on both axes (21x21).
  {
    auto px = render([] {
      afterhours::draw_rectangle(RectangleType{10.4f, 10.4f, 20.3f, 20.3f},
                                 WHITE);
    });
    int min_x = W, max_x = -1, min_y = H, max_y = -1;
    long n = 0;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++)
        if (lit(px, x, y)) {
          min_x = std::min(min_x, x); max_x = std::max(max_x, x);
          min_y = std::min(min_y, y); max_y = std::max(max_y, y);
          n++;
        }
    printf("  #80 bbox: x %d..%d, y %d..%d, %ld px (expect 10..30, 441)\n",
           min_x, max_x, min_y, max_y, n);
    check(min_x == 10 && max_x == 30 && min_y == 10 && max_y == 30 &&
              n == 441,
          "#80 rect covers exactly the pixels its geometry spans");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
