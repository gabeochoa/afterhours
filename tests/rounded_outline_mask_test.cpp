// rounded_outline_mask_test.cpp
// Pixel proofs that a partial corner mask means the same thing for a
// rounded fill and its outline, in whichever backend compiles this file
// (raylib via RAYLIB_TESTS, sokol via sokol_rounded_outline_mask_test).
//
// Mask under test: only the top-left corner rounded (bit 3). A rounded
// corner cuts the extreme corner pixel and curves through the diagonal;
// a sharp corner keeps the extreme corner pixel lit.

#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_VULKAN) && \
    !defined(AFTER_HOURS_USE_SOKOL)
#define AFTER_HOURS_USE_SOKOL
#endif

#include <afterhours/src/graphics.h>
#include <afterhours/src/drawing_helpers.h>
#include <afterhours/src/plugins/ui/rounded_corners.h>

#include <bitset>
#include <cstdio>
#include <string>
#include <vector>

namespace g = afterhours::graphics;
using afterhours::Color;
using ::RectangleType;

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
static const Color kBg{0, 0, 0, 255};
static const Color kFg{255, 255, 255, 255};

template <typename Draw> static std::vector<uint8_t> render(Draw draw) {
  g::begin_frame();
  afterhours::draw_rectangle(RectangleType{0, 0, W, H}, kBg);
  draw();
  g::end_frame();
  auto shot = afterhours::capture_render_texture_rgba(g::get_render_texture());
  if (!shot) return {};
  return shot->pixels;
}

static bool lit(const std::vector<uint8_t> &px, int x, int y) {
  const size_t i = (static_cast<size_t>(y) * W + x) * 4;
  return px.size() == static_cast<size_t>(W) * H * 4 && px[i] > 127;
}

static void probe(const char *label, const std::vector<uint8_t> &px, int x,
                  int y) {
  printf("  %-28s (%3d,%3d) = %s\n", label, x, y, lit(px, x, y) ? "lit" : "dark");
}

int main() {
  printf("=== rounded outline mask tests ===\n\n");
#ifdef AFTER_HOURS_USE_RAYLIB
  printf("backend: raylib\n");
#else
  printf("backend: sokol\n");
#endif

  // Helper: top_round must round the top corners only.
  {
    using namespace afterhours::ui::imm;
    const auto bits = RoundedCorners().top_round().get();
    check(bits == std::bitset<4>(0b1100),
          "top_round rounds TL+TR and sharpens BL+BR");
    printf("  top_round bits = %s (want 1100)\n", bits.to_string().c_str());
  }

  g::Config cfg;
  cfg.display = g::DisplayMode::Headless;
  cfg.width = W;
  cfg.height = H;
  cfg.title = "rounded outline mask test";
  if (!g::init(cfg)) {
    printf("SKIP: headless init failed (no GPU available)\n");
    printf("\n%d/%d checks passed\n", checks_passed, checks_run);
    return checks_passed == checks_run ? 0 : 1;
  }

  const RectangleType rect{40, 40, 80, 60};
  std::bitset<4> only_tl;
  only_tl.set(3);

  // Fill, only TL rounded: radius = 30 at roundness 1. The cut region
  // probe (44,44) sits outside the TL arc but inside a square corner.
  {
    auto px = render([&] {
      afterhours::draw_rectangle_rounded(rect, 1.0f, 8, kFg, only_tl);
    });
    printf("fill only-TL-rounded:\n");
    probe("TL cut region (44,44)", px, 44, 44);
    probe("TR corner (115,44)", px, 115, 44);
    probe("BL corner (44,95)", px, 44, 95);
    probe("BR corner (115,95)", px, 115, 95);
    check(!lit(px, 44, 44), "fill: rounded TL corner is cut");
    check(lit(px, 115, 44), "fill: sharp TR corner stays square");
    check(lit(px, 44, 95), "fill: sharp BL corner stays square");
    check(lit(px, 115, 95), "fill: sharp BR corner stays square");
  }

  // Outline, only TL rounded: no ink may reach the TL corner, the arc
  // must pass near the diagonal point (49,49), and every sharp corner
  // keeps ink at its vertex. Windows, not single pixels: the two
  // backends rasterize a 1px line on opposite sides of the ideal edge.
  {
    auto px = render([&] {
      afterhours::draw_rectangle_rounded_lines(rect, 1.0f, 8, kFg, only_tl);
    });
    auto any_lit = [&](int x0, int y0, int x1, int y1) {
      for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
          if (lit(px, x, y)) return true;
      return false;
    };
    printf("outline only-TL-rounded:\n");
    probe("TL extreme (40,40)", px, 40, 40);
    probe("TL arc (49,49)", px, 49, 49);
    probe("TR extreme (119,40)", px, 119, 40);
    probe("BL extreme (40,99)", px, 40, 99);
    probe("BR extreme (119,99)", px, 119, 99);
    check(!any_lit(38, 38, 44, 44), "outline: rounded TL corner is cut");
    check(any_lit(45, 45, 53, 53),
          "outline: TL arc passes through the diagonal");
    check(any_lit(78, 39, 82, 41), "outline: top edge is drawn");
    check(any_lit(39, 78, 41, 82), "outline: left edge is drawn");
    check(any_lit(118, 39, 120, 41), "outline: sharp TR corner stays square");
    check(any_lit(39, 98, 41, 100), "outline: sharp BL corner stays square");
    check(any_lit(118, 98, 120, 100),
          "outline: sharp BR corner stays square");
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
