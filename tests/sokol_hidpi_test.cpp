// sokol_hidpi_test.cpp
// D19: headless capture ignored Config.hidpi, so screenshots came out 1x and
// soft. With hidpi the target is denser while drawing stays in logical coords,
// giving a true supersample rather than a bigger canvas.
//
// Its own binary, not a section of sokol_blend_test, because the Metal backend
// cannot be re-initialised: after shutdown() a second init() hands back a
// render target that never receives any draw, and readback comes out uniform
// magenta. Sokol reports that target as valid and byte-identical to a working
// one, so the failure surfaces as whichever assertion happens to run second.
// This test therefore owns the process and calls init() exactly once. See
// docs/POLISH_PASS_AFTERHOURS_GAPS.md for the re-init bug itself.
//
// macOS/Metal only -- registered in tests/Makefile behind UNAME_S == Darwin.

#define AFTER_HOURS_USE_METAL

#include <afterhours/src/graphics.h>
#include <afterhours/src/backends/sokol/drawing_helpers.h>

#include <cstdio>
#include <string>
#include <vector>

namespace g = afterhours::graphics;
using afterhours::Color;
// RectangleType is at global scope (developer.h), not in afterhours::.

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

static bool near(int a, int b, int tol = 6) {
  return (a > b ? a - b : b - a) <= tol;
}

static constexpr int W = 200;
static constexpr int H = 200;

struct Px {
  int r, g, b, a;
};

int main() {
  printf("=== sokol hidpi tests ===\n\n");

  g::Config hi;
  hi.display = g::DisplayMode::Headless;
  hi.width = W;
  hi.height = H;
  hi.hidpi = true;
  hi.title = "hidpi test";
  if (!g::init(hi)) {
    // No GPU (CI container, remote session). Skip rather than fail: a red
    // suite for "this machine has no Metal device" teaches nothing.
    printf("SKIP: hidpi headless init failed (no GPU available)\n");
    return 0;
  }

  const Color opaque_blue{0, 0, 255, 255};
  const Color opaque_green{0, 255, 0, 255};

  const int scale = g::render_scale();
  check(scale == 2, "hidpi headless sets a 2x render scale");

  auto &rt = g::metal_detail::g_headless_rt;
  check(rt.width == W * 2 && rt.height == H * 2,
        "the offscreen target is allocated at 2x");
  check(rt.scale == 2 && rt.width / rt.scale == W &&
            rt.height / rt.scale == H,
        "the target reports its scale, so logical size is derivable");

  // Fill the LEFT HALF in logical coords. If the projection were physical, it
  // would only cover a quarter of the image.
  g::begin_drawing();
  afterhours::draw_rectangle(RectangleType{0, 0, W, H}, opaque_blue);
  afterhours::draw_rectangle(RectangleType{0, 0, W / 2, H}, opaque_green);
  g::end_drawing();

  std::vector<uint8_t> px = afterhours::capture_render_texture_to_memory(rt);
  check(px.size() == static_cast<size_t>(W * 2) * (H * 2) * 4,
        "capture returns the full 2x pixel buffer");
  if (px.size() == static_cast<size_t>(W * 2) * (H * 2) * 4) {
    const int pw = W * 2;
    auto at = [&](int x, int y) {
      const size_t i = (static_cast<size_t>(y) * pw + x) * 4;
      return Px{px[i], px[i + 1], px[i + 2], px[i + 3]};
    };
    const Px left = at(pw / 4, H);      // inside the logical left half
    const Px right = at(pw * 3 / 4, H); // inside the logical right half
    check(near(left.g, 255) && near(left.r, 0),
          "logical left half is green across the 2x image");
    check(near(right.b, 255) && near(right.g, 0),
          "logical right half is blue across the 2x image");
    if (!near(left.g, 255))
      fprintf(stderr, "        left  rgba(%d, %d, %d, %d)\n", left.r, left.g,
              left.b, left.a);
    if (!near(right.b, 255))
      fprintf(stderr, "        right rgba(%d, %d, %d, %d)\n", right.r, right.g,
              right.b, right.a);
  }

  g::shutdown();

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
