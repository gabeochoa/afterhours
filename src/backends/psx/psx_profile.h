#pragma once

#include <cstddef>


namespace afterhours::psx {

enum class Console { PS1, PS2 };

struct RenderSetup {
  int width = 320;
  int height = 240;
  int color_depth_bpp = 16;
  bool double_buffered = true;
  bool interlaced = false;
};

struct ConsoleProfile {
  const char *name = "custom";

  std::size_t vram_bytes = 0;
  int vram_width_px = 0;
  int vram_height_px = 0;

  std::size_t main_ram_bytes = 0;

  int max_texture_width = 0;
  int max_texture_height = 0;

  bool textures_use_clut = false;

  int default_texture_bpp = 8;

  std::size_t primitive_budget_per_frame = 0;

  RenderSetup render;
};

inline ConsoleProfile ps1_profile() {
  ConsoleProfile p;
  p.name = "PlayStation 1";
  p.vram_bytes = std::size_t{1} << 20;
  p.vram_width_px = 1024;
  p.vram_height_px = 512;
  p.main_ram_bytes = std::size_t{2} << 20;
  p.max_texture_width = 256;
  p.max_texture_height = 256;
  p.textures_use_clut = true;
  p.default_texture_bpp = 8;
  p.primitive_budget_per_frame = 3000;
  p.render = RenderSetup{.width = 320,
                         .height = 240,
                         .color_depth_bpp = 16,
                         .double_buffered = true,
                         .interlaced = false};
  return p;
}

inline ConsoleProfile ps2_profile() {
  ConsoleProfile p;
  p.name = "PlayStation 2";
  p.vram_bytes = std::size_t{4} << 20;
  p.vram_width_px = 2048;
  p.vram_height_px = 1024;
  p.main_ram_bytes = std::size_t{32} << 20;
  p.max_texture_width = 1024;
  p.max_texture_height = 1024;
  p.textures_use_clut = false;
  p.default_texture_bpp = 32;
  p.primitive_budget_per_frame = 20000;
  p.render = RenderSetup{.width = 640,
                         .height = 448,
                         .color_depth_bpp = 32,
                         .double_buffered = true,
                         .interlaced = true};
  return p;
}

inline ConsoleProfile profile_for(Console console) {
  return console == Console::PS2 ? ps2_profile() : ps1_profile();
}

inline int units_for_row(int width_px, int bpp) {
  if (width_px <= 0 || bpp <= 0)
    return 0;
  const int bits = width_px * bpp;
  return (bits + 15) / 16;
}

inline std::size_t texture_bytes(int width_px, int height_px, int bpp) {
  if (width_px <= 0 || height_px <= 0)
    return 0;
  return static_cast<std::size_t>(units_for_row(width_px, bpp)) *
         static_cast<std::size_t>(height_px) * 2u;
}

inline int clut_units(int bpp) {
  if (bpp == 4)
    return 16;
  if (bpp == 8)
    return 256;
  return 0;
}

}
