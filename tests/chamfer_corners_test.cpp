// chamfer_corners_test.cpp
// Pixel proof that a chamfered corner is a straight 45-degree cut whose
// endpoints sit one radius along each edge, in whichever backend compiles
// this file (raylib via RAYLIB_TESTS, sokol via sokol_chamfer_corners_test).
//
// Rect under test: (40,40) 80x60, roundness 1 -> radius 30. TL corner cut
// points are (40,70) and (70,40); the diagonal is the line x+y = 110. A
// rounded corner of the same radius passes through (49,49) and keeps
// (47,62) lit; a chamfer keeps nothing above the diagonal lit.

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
  printf("=== chamfer corners tests ===\n\n");
#ifdef AFTER_HOURS_USE_RAYLIB
  printf("backend: raylib\n");
#else
  printf("backend: sokol\n");
#endif

  // Helper: chamfer mask selects bit 3 only for the top-left corner.
  {
    using namespace afterhours::ui::imm;
    const auto bits = RoundedCorners().chamfer(TOP_LEFT).get_chamfered();
    check(bits == std::bitset<4>(0b1000), "chamfer(TOP_LEFT) sets bit 3 only");
    printf("  chamfer bits = %s (want 1000)\n", bits.to_string().c_str());
  }

  g::Config cfg;
  cfg.display = g::DisplayMode::Headless;
  cfg.width = W;
  cfg.height = H;
  cfg.title = "chamfer corners test";
  if (!g::init(cfg)) {
    printf("SKIP: headless init failed (no GPU available)\n");
    printf("\n%d/%d checks passed\n", checks_passed, checks_run);
    return checks_passed == checks_run ? 0 : 1;
  }

  const RectangleType rect{40, 40, 80, 60};
  std::bitset<4> only_tl;
  only_tl.set(3);

  // Fill, only TL chamfered: nothing with x+y < 110 in the TL 30x30 box is
  // lit, and the sharp corners stay square.
  {
    auto px = render([&] {
      afterhours::draw_rectangle_rounded(rect, 1.0f, 8, kFg, std::bitset<4>(),
                                         only_tl);
    });
    printf("fill only-TL-chamfered:\n");
    probe("TL extreme (40,40)", px, 40, 40);
    probe("TL cut (44,44)", px, 44, 44);
    probe("below diagonal (45,62)", px, 45, 62);
    probe("above diagonal (62,45)", px, 62, 45);
    probe("inside (60,60)", px, 60, 60);
    probe("TR corner (119,40)", px, 119, 40);
    probe("BL corner (40,99)", px, 40, 99);
    probe("BR corner (119,99)", px, 119, 99);
    check(!lit(px, 40, 40), "fill: chamfered TL corner is cut");
    check(!lit(px, 44, 44), "fill: pixel above the diagonal is cut");
    check(!lit(px, 45, 62), "fill: 45-degree cut (sum 107 < 110)");
    check(!lit(px, 62, 45), "fill: 45-degree cut (symmetric)");
    check(lit(px, 60, 60), "fill: pixel below the diagonal stays lit");
    check(lit(px, 119, 40), "fill: sharp TR corner stays square");
    check(lit(px, 40, 99), "fill: sharp BL corner stays square");
    check(lit(px, 119, 99), "fill: sharp BR corner stays square");
  }

  // Fill, TL rounded (no chamfer): the arc keeps (47,62) lit where a
  // chamfer of the same radius cuts it. Proves the two styles differ and
  // that rounded rendering is unchanged.
  {
    auto px = render([&] {
      afterhours::draw_rectangle_rounded(rect, 1.0f, 8, kFg, only_tl);
    });
    printf("fill only-TL-rounded:\n");
    probe("TL cut region (44,44)", px, 44, 44);
    probe("arc region (47,62)", px, 47, 62);
    check(!lit(px, 44, 44), "fill: rounded TL corner is cut");
    check(lit(px, 47, 62), "fill: rounded TL corner keeps (47,62) lit");
  }

  // Outline, only TL chamfered: the diagonal itself carries ink near its
  // midpoint and nothing reaches the extreme corner.
  {
    auto px = render([&] {
      afterhours::draw_rectangle_rounded_lines(rect, 1.0f, 8, kFg,
                                               std::bitset<4>(), only_tl);
    });
    auto any_lit = [&](int x0, int y0, int x1, int y1) {
      for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
          if (lit(px, x, y)) return true;
      return false;
    };
    printf("outline only-TL-chamfered:\n");
    probe("TL extreme (40,40)", px, 40, 40);
    probe("TL diagonal (55,55)", px, 55, 55);
    probe("TR extreme (119,40)", px, 119, 40);
    check(!any_lit(38, 38, 44, 44), "outline: chamfered TL corner is cut");
    check(any_lit(50, 50, 60, 60),
          "outline: diagonal passes through the middle of the cut");
    check(any_lit(118, 39, 120, 41), "outline: sharp TR corner stays square");
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
