// psx_budget_test.cpp
// The PSX budget tracker (src/backends/psx/budget.h) without the PSX
// backend: profiles, the VRAM allocator, and the accounting rules are
// backend-independent on purpose, so a raylib/sokol game can budget against
// a console today. This test deliberately does NOT define
// AFTER_HOURS_USE_PSX to keep that true. psx_backend_test.cpp covers the
// backend integration.

#include <afterhours/src/backends/psx/budget.h>

#include <cstdio>
#include <string>

using namespace afterhours;
namespace budget = psx::budget;

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

int main() {
  // --- profile numbers -----------------------------------------------------
  const psx::ConsoleProfile ps1 = psx::ps1_profile();
  check(ps1.vram_bytes == (std::size_t{1} << 20), "PS1 VRAM is 1MB");
  check(ps1.vram_width_px == 1024 && ps1.vram_height_px == 512,
        "PS1 VRAM grid is 1024x512");
  check(static_cast<std::size_t>(ps1.vram_width_px) * ps1.vram_height_px * 2 ==
            ps1.vram_bytes,
        "PS1 grid and byte size agree");
  check(ps1.max_texture_width == 256 && ps1.max_texture_height == 256,
        "PS1 max texture is one 256x256 page");

  const psx::ConsoleProfile ps2 = psx::ps2_profile();
  check(ps2.vram_bytes == (std::size_t{4} << 20), "PS2 VRAM is 4MB");
  check(static_cast<std::size_t>(ps2.vram_width_px) * ps2.vram_height_px * 2 ==
            ps2.vram_bytes,
        "PS2 grid and byte size agree");
  check(psx::profile_for(psx::Console::PS2).vram_bytes == ps2.vram_bytes,
        "profile_for(PS2) matches ps2_profile");

  // --- unit math -----------------------------------------------------------
  check(psx::units_for_row(256, 8) == 128, "8bpp packs 2px per 16bpp unit");
  check(psx::units_for_row(256, 4) == 64, "4bpp packs 4px per unit");
  check(psx::units_for_row(256, 16) == 256, "16bpp is 1 unit per px");
  check(psx::units_for_row(256, 32) == 512, "32bpp is 2 units per px");
  check(psx::units_for_row(3, 4) == 1, "odd widths round up to whole units");
  check(psx::texture_bytes(256, 256, 8) == 65536, "256x256 @8bpp is 64KB");
  check(psx::clut_units(8) == 256 && psx::clut_units(4) == 16 &&
            psx::clut_units(16) == 0,
        "CLUT sizes follow bit depth");

  // --- allocator -----------------------------------------------------------
  psx::VramAllocator vram(64, 32);
  check(vram.total_bytes() == 64u * 32u * 2u, "allocator byte total");
  auto a = vram.allocate(32, 32);
  check(a.has_value() && a->x == 0 && a->y == 0, "first alloc lands at origin");
  auto b = vram.allocate(32, 32);
  check(b.has_value() && b->x == 32, "second alloc packs beside the first");
  check(!vram.allocate(32, 32).has_value(), "full grid refuses a third");
  check(!vram.allocate(65, 1).has_value(), "wider than VRAM refuses");
  check(vram.fraction_used() == 1.0f, "fraction used reaches 1 when full");
  vram.free(*a);
  auto c = vram.allocate(16, 16);
  check(c.has_value() && c->x == 0 && c->y == 0, "freed hole is reused");
  check(!vram.reserve(psx::VramRect{40, 0, 8, 8}),
        "reserve over occupied cells fails");
  check(vram.reserve(psx::VramRect{16, 16, 8, 8}),
        "reserve into free cells succeeds");
  const std::string map = vram.map_ascii(64, 32);
  check(map.find('#') != std::string::npos &&
            map.find('.') != std::string::npos,
        "ascii map shows both states");

  // --- budget: framebuffers come first -------------------------------------
  budget::configure({.console = psx::Console::PS1});
  budget::Report r = budget::report();
  // 320x240 @16bpp double buffered = 2 * 150KB.
  check(r.framebuffer_bytes == 2u * 320u * 240u * 2u,
        "PS1 framebuffers reserve 300KB");
  check(r.vram_used_bytes == r.framebuffer_bytes,
        "only framebuffers are used after configure");
  check(r.vram_total_bytes == (std::size_t{1} << 20), "report totals 1MB");

  // --- budget: texture accounting ------------------------------------------
  auto t1 = budget::alloc_texture(256, 256, "hero.png");
  check(t1.ok && t1.id != 0, "256x256 texture fits on PS1");
  check(t1.bytes == 65536 + 512, "8bpp texture charges image + 256-entry CLUT");
  check(t1.clut_rect.valid(), "paletted texture got a CLUT placement");
  check(budget::report().texture_count == 1, "texture count tracks alloc");

  auto big = budget::alloc_texture(512, 256, "too-wide.png");
  check(big.oversized, "512-wide texture is oversized for PS1");
  check(big.ok, "non-strict oversized texture still tracked");
  check(budget::report().oversized_alloc_count == 1, "oversized counted");

  budget::free_texture(t1.id);
  check(budget::report().texture_count == 1,
        "free drops the texture count (big one remains)");

  // Fill VRAM: 256x256 @15bpp-ish direct textures at 128KB each cannot all
  // fit beside 300KB of framebuffers in 1MB.
  budget::configure({.console = psx::Console::PS1});
  int placed = 0;
  for (int i = 0; i < 16; ++i) {
    auto t = budget::alloc_texture(256, 256, "fill", 16);
    if (t.ok)
      ++placed;
  }
  r = budget::report();
  check(placed > 0 && placed < 16, "VRAM fills before 16x128KB textures fit");
  check(r.failed_alloc_count > 0, "overflow allocs counted as failed");
  check(r.overspill_bytes > 0, "overspill bytes report how far over we are");
  check(r.vram_fraction_used > 0.9f, "VRAM reports nearly full");

  // --- budget: strict mode ---------------------------------------------------
  budget::configure({.console = psx::Console::PS1, .strict = true});
  auto wide = budget::alloc_texture(1024, 64, "backdrop.png");
  check(wide.oversized && !wide.ok && wide.id == 0,
        "strict mode refuses an oversized texture outright");

  // --- budget: render targets and PS2 ---------------------------------------
  budget::configure({.console = psx::Console::PS2});
  r = budget::report();
  // 640x448 @32bpp double buffered.
  check(r.framebuffer_bytes == 2u * 640u * 448u * 4u,
        "PS2 framebuffers reserve 640x448 @32bpp x2");
  // Not fullscreen: two 640-wide framebuffers stack down the left of the
  // grid and leave a 384px-wide strip, so a third 640-wide target has the
  // bytes but not the contiguous room. (The real GS addresses texture
  // memory by pages and is more forgiving; the 2D model is the strict
  // reading -- see psx_profile.h.)
  auto rt = budget::alloc_render_target(384, 448, "post");
  check(rt.ok, "PS2 fits an extra render target beside its framebuffers");
  check(budget::report().render_target_count == 1, "render targets counted");
  auto huge = budget::alloc_texture(1024, 1024, "skybox.png");
  check(!huge.oversized, "1024x1024 is legal on PS2");

  budget::configure({.console = psx::Console::PS1});
  auto rt_ps1 = budget::alloc_render_target(320, 240, "post");
  check(rt_ps1.ok, "one extra 320x240 target fits on PS1");
  bool exhausted = false;
  for (int i = 0; i < 8 && !exhausted; ++i)
    exhausted = !budget::alloc_render_target(320, 240, "post").ok;
  check(exhausted, "stacking fullscreen targets exhausts PS1 VRAM");

  // --- budget: render setup override -----------------------------------------
  budget::configure({.console = psx::Console::PS1,
                     .render_override = psx::RenderSetup{.width = 512,
                                                         .height = 240,
                                                         .color_depth_bpp = 16,
                                                         .double_buffered =
                                                             true}});
  check(budget::report().framebuffer_bytes == 2u * 512u * 240u * 2u,
        "render override drives the framebuffer reservation");

  // 640x480 double-buffered @16bpp is 1.2MB into 1MB of VRAM: the second
  // buffer has nowhere to go, on paper or on hardware.
  budget::configure({.console = psx::Console::PS1,
                     .render_override = psx::RenderSetup{.width = 640,
                                                         .height = 480,
                                                         .color_depth_bpp = 16,
                                                         .double_buffered =
                                                             true}});
  check(budget::report().framebuffer_bytes == 640u * 480u * 2u,
        "PS1 640x480 double-buffered only fits one buffer");

  // --- budget: primitives ------------------------------------------------------
  budget::configure({.console = psx::Console::PS1});
  budget::add_primitives(2500);
  budget::end_frame();
  check(budget::report().primitives_last_frame == 2500,
        "primitive count lands in last frame");
  check(budget::report().frames_over_primitive_budget == 0,
        "2500 prims is under the PS1 budget");
  budget::add_primitives(9000);
  budget::end_frame();
  r = budget::report();
  check(r.primitives_peak_frame == 9000, "peak frame tracked");
  check(r.frames_over_primitive_budget == 1, "over-budget frame counted");

  // --- report string / listing -------------------------------------------------
  budget::configure({.console = psx::Console::PS1});
  budget::alloc_texture(128, 128, "slime.png");
  const std::string summary = budget::report_string();
  check(summary.find("PlayStation 1") != std::string::npos,
        "report string names the console");
  const auto listing = budget::alloc_listing();
  check(listing.size() == 1 &&
            listing[0].find("slime.png") != std::string::npos,
        "alloc listing names live textures");

  budget::reset();
  check(budget::report().texture_count == 0, "reset frees every texture");

  std::printf("psx_budget_test: %d/%d checks passed\n", checks_passed,
              checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
