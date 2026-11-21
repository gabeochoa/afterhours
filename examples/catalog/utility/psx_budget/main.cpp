// psx_budget: run a game's worth of texture loads against a PlayStation 1
// VRAM budget, then against a PlayStation 2 budget, and watch the same art
// fail one and pass the other.
//
// Built with AFTER_HOURS_USE_PSX, so load_texture_from_rgba and friends
// charge the simulated console VRAM in src/backends/psx/budget.h. There is
// no window and no GPU: draw calls record, textures are footprints, and the
// output is the budget report plus the VRAM map an emulator would show you.
//
// The budget half works without the backend too -- a raylib game can call
// psx::budget::alloc_texture/free_texture around its own loads and get the
// same warnings. This example uses the backend because it needs no window.

#include <cstdio>

#include "../../../../ah.h"
#include "../../../../src/drawing_helpers.h"
#include "../../../../src/font_helper.h"

using namespace afterhours;
namespace budget = psx::budget;

static void load_a_level() {
  // What a small 2D game loads for one level. Sizes are the point; on the
  // PSX backend the pixels are never read.
  TextureType hero = load_texture_from_rgba(nullptr, 256, 256);
  TextureType tiles = load_texture_from_rgba(nullptr, 256, 256);
  TextureType enemies = load_texture_from_rgba(nullptr, 256, 128);
  TextureType hud = load_texture_from_rgba(nullptr, 256, 64);
  Font ui_font = load_font_from_file("ui.ttf"); // one 256x256 atlas page
  (void)ui_font;

  // One frame's worth of drawing charges the primitive budget.
  for (int i = 0; i < 40; ++i)
    draw_texture_rec(tiles, RectangleType{0, 0, 32, 32},
                     Vector2Type{static_cast<float>(i * 8), 200},
                     Color{255, 255, 255, 255});
  draw_texture_rec(hero, RectangleType{0, 0, 32, 48}, Vector2Type{40, 150},
                   Color{255, 255, 255, 255});
  draw_text("SCORE 012340", 8, 8, 16, Color{255, 255, 0, 255});
  budget::end_frame();

  // ... and then the artist hands you a backdrop painted at desktop size.
  TextureType backdrop = load_texture_from_rgba(nullptr, 1024, 512);

  std::printf("%s\n", budget::report_string().c_str());
  for (const std::string &line : budget::alloc_listing())
    std::printf("  %s\n", line.c_str());
  std::printf("%s\n\n", budget::vram_map_ascii(64, 16).c_str());

  unload_texture(hero);
  unload_texture(tiles);
  unload_texture(enemies);
  unload_texture(hud);
  unload_texture(backdrop);
}

int main() {
  std::printf("== Aiming at PlayStation 1 (1MB VRAM, 256x256 textures) ==\n");
  budget::configure({.console = psx::Console::PS1});
  load_a_level();

  std::printf("== Same art, PlayStation 2 (4MB VRAM, 1024x1024 textures) ==\n");
  // texture_bpp 8: plenty of PS2 art shipped 8bpp paletted (the profile
  // default, 32bpp direct, would charge the backdrop alone 2MB and blow
  // this budget too -- try it).
  budget::configure({.console = psx::Console::PS2, .texture_bpp = 8});
  load_a_level();

  return 0;
}
