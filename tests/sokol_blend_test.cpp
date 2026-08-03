// sokol_blend_test.cpp
// D1: the sokol backend had alpha blending disabled. sgl_defaults() loads
// sokol_gl's default pipeline and sokol_gfx defaults blending OFF, so every
// colour's alpha byte reached sgl_c4b and was then discarded -- translucent
// backgrounds filled opaque and transparent texture texels blitted as black.
// Three separately-filed symptoms across two downstream apps, one cause.
//
// This is the first sokol test binary in the repo. It renders headless (no
// window, no WindowServer) into an offscreen texture and reads the pixels back,
// so the assertion is on what the GPU actually produced rather than on which
// calls were issued.
//
// macOS/Metal only -- registered in tests/Makefile behind UNAME_S == Darwin.

#define AFTER_HOURS_USE_METAL

#include <afterhours/src/graphics.h>
#include <afterhours/src/backends/sokol/drawing_helpers.h>

#include "ui_test_harness.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace g = afterhours::graphics;
namespace ui = afterhours::ui;
namespace imm = afterhours::ui::imm;
using afterhours::Color;
using afterhours::Entity;
using afterhours::EntityHelper;
using afterhours::ui::ComponentSize;
using afterhours::ui::pixels;
using afterhours::ui::imm::ComponentConfig;
using afterhours::ui::imm::mk;
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

static constexpr int W = 200;
static constexpr int H = 200;

struct Px {
  int r, g, b, a;
};

// Render one frame through the real backend and read back the centre pixel.
static bool render_and_sample(const Color &under, const Color &over, Px &out) {
  g::begin_drawing();
  // Fill twice rather than using a clear colour: begin_texture_mode loads the
  // existing contents (SG_LOADACTION_LOAD), and we want a known backdrop that
  // the second draw composites against.
  afterhours::draw_rectangle(RectangleType{0, 0, W, H}, under);
  afterhours::draw_rectangle(RectangleType{0, 0, W, H}, over);
  g::end_drawing();

  // Raw RGBA, w*h*4 -- not PNG (capture_impl.h), so index it directly.
  std::vector<uint8_t> px = afterhours::capture_render_texture_to_memory(
      g::metal_detail::g_headless_rt);
  if (px.size() != static_cast<size_t>(W) * H * 4) {
    fprintf(stderr, "  capture returned %zu bytes, expected %d\n", px.size(),
            W * H * 4);
    return false;
  }
  const size_t i = (static_cast<size_t>(H / 2) * W + (W / 2)) * 4;
  out = {px[i], px[i + 1], px[i + 2], px[i + 3]};
  return true;
}

static bool near(int a, int b, int tol = 6) {
  return (a > b ? a - b : b - a) <= tol;
}

// Same thing one layer up: build a real imm widget with a translucent
// background, run the real renderer, and sample the result. This is the level
// the bug was reported at.
static bool render_ui_over_blue(Px &out) {
  ui_test::ImmTestHarness h;
  imm::div(h.context(), mk(h.root(), 0),
           ComponentConfig{}
               .with_size(ComponentSize{pixels(W), pixels(H)})
               .with_custom_background(Color{255, 0, 0, 128}));
  h.layout_only();

  Entity &font_ent = EntityHelper::createPermanentEntity();
  auto &fm = font_ent.addComponent<ui::FontManager>();
  fm.load_font(ui::UIComponent::DEFAULT_FONT, afterhours::get_default_font());

  g::begin_drawing();
  afterhours::draw_rectangle(RectangleType{0, 0, W, H}, Color{0, 0, 255, 255});
  ui::RenderImm<ui_test::TestInputAction> renderer;
  renderer.for_each_with_derived(h.root(), h.context(), fm, 0.f);
  g::end_drawing();

  std::vector<uint8_t> px = afterhours::capture_render_texture_to_memory(
      g::metal_detail::g_headless_rt);
  if (px.size() != static_cast<size_t>(W) * H * 4)
    return false;
  const size_t i = (static_cast<size_t>(H / 2) * W + (W / 2)) * 4;
  out = {px[i], px[i + 1], px[i + 2], px[i + 3]};
  return true;
}

int main() {
  printf("=== sokol blend tests ===\n\n");

  g::Config cfg;
  cfg.display = g::DisplayMode::Headless;
  cfg.width = W;
  cfg.height = H;
  cfg.title = "blend test";
  if (!g::init(cfg)) {
    // No GPU (CI container, remote session). Skip rather than fail: a red
    // suite for "this machine has no Metal device" teaches nothing.
    printf("SKIP: headless Metal init failed (no GPU available)\n");
    return 0;
  }

  const Color opaque_blue{0, 0, 255, 255};
  const Color half_red{255, 0, 0, 128};
  const Color opaque_green{0, 255, 0, 255};

  // 1. The bug. Blue underneath, half-alpha red on top.
  //    Blended  -> ~(128, 0, 127): src*a + dst*(1-a).
  //    Unblended -> exactly (255, 0, 0), the alpha byte thrown away.
  {
    Px p{};
    if (render_and_sample(opaque_blue, half_red, p)) {
      check(!(p.r == 255 && p.g == 0 && p.b == 0),
            "half-alpha red does not overwrite as pure red");
      check(near(p.r, 128) && near(p.g, 0) && near(p.b, 127),
            "half-alpha red composites over blue");
      if (!(near(p.r, 128) && near(p.b, 127)))
        fprintf(stderr, "        got rgba(%d, %d, %d, %d)\n", p.r, p.g, p.b,
                p.a);
    } else {
      check(false, "captured a frame for the blended case");
    }
  }

  // 2. Opaque drawing must be untouched. This is what makes enabling blending
  //    safe as a global default: with a == 255, src-over is src*1 + dst*0.
  {
    Px p{};
    if (render_and_sample(opaque_blue, opaque_green, p)) {
      check(near(p.r, 0) && near(p.g, 255) && near(p.b, 0),
            "opaque draw still fully replaces what is under it");
      if (!near(p.g, 255))
        fprintf(stderr, "        got rgba(%d, %d, %d, %d)\n", p.r, p.g, p.b,
                p.a);
    } else {
      check(false, "captured a frame for the opaque case");
    }
  }

  // 3. Fully transparent source must leave the backdrop alone -- the texture
  //    case, where a=0 texels were blitting as black.
  {
    Px p{};
    if (render_and_sample(opaque_blue, Color{255, 0, 0, 0}, p)) {
      check(near(p.b, 255) && near(p.r, 0),
            "a fully transparent draw leaves the backdrop unchanged");
      if (!near(p.b, 255))
        fprintf(stderr, "        got rgba(%d, %d, %d, %d)\n", p.r, p.g, p.b,
                p.a);
    } else {
      check(false, "captured a frame for the transparent case");
    }
  }

  // 4. End to end through the UI layer, which is where this was actually
  //    reported: with_custom_background(Color{r,g,b,a<255}) filled opaque and
  //    with_opacity() had no effect. Cases 1-3 drive draw_rectangle directly,
  //    so they would still pass if the fix somehow did not reach imm widgets.
  {
    Px p{};
    if (render_ui_over_blue(p)) {
      check(!(p.r == 255 && p.g == 0 && p.b == 0),
            "a translucent div background is not filled opaque");
      check(near(p.r, 128, 12) && near(p.b, 127, 12),
            "with_custom_background alpha composites over what is behind it");
      if (!near(p.r, 128, 12))
        fprintf(stderr, "        got rgba(%d, %d, %d, %d)\n", p.r, p.g, p.b,
                p.a);
    } else {
      check(false, "rendered a UI frame");
    }
  }

  g::shutdown();

  // D19: headless capture ignored Config.hidpi, so screenshots came out 1x and
  // soft. With hidpi the target is denser while drawing stays in logical
  // coords, giving a true supersample rather than a bigger canvas.
  {
    g::Config hi;
    hi.display = g::DisplayMode::Headless;
    hi.width = W;
    hi.height = H;
    hi.hidpi = true;
    hi.title = "hidpi test";
    if (g::init(hi)) {
      const int scale = g::render_scale();
      check(scale == 2, "hidpi headless sets a 2x render scale");

      auto &rt = g::metal_detail::g_headless_rt;
      check(rt.width == W * 2 && rt.height == H * 2,
            "the offscreen target is allocated at 2x");
      check(rt.scale == 2 && rt.width / rt.scale == W &&
                rt.height / rt.scale == H,
            "the target reports its scale, so logical size is derivable");

      // Fill the LEFT HALF in logical coords. If the projection were physical,
      // it would only cover a quarter of the image.
      g::begin_drawing();
      afterhours::draw_rectangle(RectangleType{0, 0, W, H}, opaque_blue);
      afterhours::draw_rectangle(RectangleType{0, 0, W / 2, H}, opaque_green);
      g::end_drawing();

      std::vector<uint8_t> px =
          afterhours::capture_render_texture_to_memory(rt);
      check(px.size() == static_cast<size_t>(W * 2) * (H * 2) * 4,
            "capture returns the full 2x pixel buffer");
      if (px.size() == static_cast<size_t>(W * 2) * (H * 2) * 4) {
        const int pw = W * 2;
        auto at = [&](int x, int y) {
          const size_t i = (static_cast<size_t>(y) * pw + x) * 4;
          return Px{px[i], px[i + 1], px[i + 2], px[i + 3]};
        };
        const Px left = at(pw / 4, H);   // inside the logical left half
        const Px right = at(pw * 3 / 4, H); // inside the logical right half
        check(near(left.g, 255) && near(left.r, 0),
              "logical left half is green across the 2x image");
        check(near(right.b, 255) && near(right.g, 0),
              "logical right half is blue across the 2x image");
      }
      g::shutdown();
    } else {
      printf("SKIP: hidpi headless init failed\n");
    }
  }

  // D20: load_texture built no mip chain, so a texture drawn much smaller than
  // its source sampled the full-res level and thin detail aliased. sokol has no
  // runtime mipmap generation, so the chain is built on the CPU at load.
  {
    // 4x4 -> 2x2 -> 1x1
    std::vector<unsigned char> solid(4 * 4 * 4, 0);
    for (int i = 0; i < 4 * 4; i++) {
      solid[i * 4 + 0] = 10;
      solid[i * 4 + 1] = 20;
      solid[i * 4 + 2] = 30;
      solid[i * 4 + 3] = 255;
    }
    const auto mips = afterhours::metal_texture_detail::build_mip_chain(solid.data(), 4, 4);
    check(mips.size() == 2, "4x4 produces two smaller levels");
    if (mips.size() == 2) {
      check(mips[0].size() == 2u * 2 * 4, "first level is 2x2");
      check(mips[1].size() == 4u, "last level is 1x1");
      check(mips[1][0] == 10 && mips[1][1] == 20 && mips[1][2] == 30,
            "a solid image stays the same colour all the way down");
    }

    // A 2x2 half-black half-white averages to mid grey, which is the whole
    // point: the small level is the average, not one of the source texels.
    std::vector<unsigned char> checker = {
        0,   0,   0,   255, 255, 255, 255, 255,
        255, 255, 255, 255, 0,   0,   0,   255};
    const auto cm = afterhours::metal_texture_detail::build_mip_chain(checker.data(), 2, 2);
    check(cm.size() == 1 && cm[0].size() == 4u, "2x2 produces one 1x1 level");
    if (cm.size() == 1)
      check(cm[0][0] == 128, "1x1 level is the average of the 2x2 block");

    // Non-square and odd sizes must terminate rather than loop or divide to 0.
    const std::vector<unsigned char> odd(5 * 3 * 4, 200);
    const auto om = afterhours::metal_texture_detail::build_mip_chain(odd.data(), 5, 3);
    check(!om.empty() && om.back().size() == 4u,
          "an odd non-square image still reduces to 1x1");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
