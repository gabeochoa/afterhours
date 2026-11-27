
#include <cassert>
#include <cstdio>

#include "../../../../ah.h"
#include "../../../../src/drawing_helpers.h"
#include "../../../../src/font_helper.h"

using namespace afterhours;
namespace budget = psx::budget;

static budget::Report load_a_level() {
  TextureType hero = load_texture_from_rgba(nullptr, 256, 256);
  TextureType tiles = load_texture_from_rgba(nullptr, 256, 256);
  TextureType enemies = load_texture_from_rgba(nullptr, 256, 128);
  TextureType hud = load_texture_from_rgba(nullptr, 256, 64);
  Font ui_font = load_font_from_file("ui.ttf");
  (void)ui_font;

  for (int i = 0; i < 40; ++i)
    draw_texture_rec(tiles, RectangleType{0, 0, 32, 32},
                     Vector2Type{static_cast<float>(i * 8), 200},
                     Color{255, 255, 255, 255});
  draw_texture_rec(hero, RectangleType{0, 0, 32, 48}, Vector2Type{40, 150},
                   Color{255, 255, 255, 255});
  draw_text("SCORE 012340", 8, 8, 16, Color{255, 255, 0, 255});
  budget::end_frame();

  TextureType backdrop = load_texture_from_rgba(nullptr, 1024, 512);

  std::printf("%s\n", budget::report_string().c_str());
  for (const std::string &line : budget::alloc_listing())
    std::printf("  %s\n", line.c_str());
  std::printf("%s\n\n", budget::vram_map_ascii(64, 16).c_str());

  const budget::Report result = budget::report();
  unload_texture(hero);
  unload_texture(tiles);
  unload_texture(enemies);
  unload_texture(hud);
  unload_texture(backdrop);
  return result;
}

int main() {
  std::printf("== Aiming at PlayStation 1 (1MB VRAM, 256x256 textures) ==\n");
  budget::configure({.console = psx::Console::PS1});
  const budget::Report ps1 = load_a_level();

  std::printf("== Same art, PlayStation 2 (4MB VRAM, 1024x1024 textures) ==\n");
  budget::configure({.console = psx::Console::PS2, .texture_bpp = 8});
  const budget::Report ps2 = load_a_level();

  assert(ps1.vram_total_bytes == (std::size_t{1} << 20));
  assert(ps1.texture_count >= 5 && ps1.vram_used_bytes > 0);
  assert(ps2.vram_total_bytes == (std::size_t{4} << 20));
  assert(ps2.vram_used_bytes > ps1.vram_used_bytes);
  return 0;
}
