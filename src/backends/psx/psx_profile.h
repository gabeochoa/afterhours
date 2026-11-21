#pragma once

#include <cstddef>

// Console profiles for the PSX backend.
//
// The PSX backend is a desktop *validation* backend: it records draw calls
// like the `none` backend, and in addition simulates the target console's
// memory so a game built against raylib/sokol-class expectations gets loud
// warnings the moment its art would not fit on real hardware. It does not
// (yet) emit code for a real PlayStation; the profile, VRAM allocator and
// budget tracker here are platform-free on purpose so a future
// PSn00bSDK-based backend can reuse them unchanged.
//
// A profile is plain data: how much VRAM there is, how it is shaped, how big
// a texture may be, and the render setup (resolution / depth / buffering)
// whose framebuffers get reserved out of that VRAM before any texture is
// placed. Pick a preset (ps1_profile / ps2_profile) or fill one in yourself;
// budget::configure() takes any of them.

namespace afterhours::psx {

enum class Console { PS1, PS2 };

// The display setup a game would run with on the console. Its framebuffers
// live in VRAM on both machines, so this is also the first VRAM reservation.
struct RenderSetup {
  int width = 320;
  int height = 240;
  int color_depth_bpp = 16;
  bool double_buffered = true;
  bool interlaced = false;
};

struct ConsoleProfile {
  const char *name = "custom";

  // VRAM is modeled as a 2D grid of 16bpp units, the way both GPUs address
  // it. PS1 VRAM really is 1024x512 at 16bpp (1MB). PS2 eDRAM is 4MB with a
  // page/column layout the GS hides behind its own addressing; it is modeled
  // here as an equivalent 2048x1024 grid. Texture *byte* costs come out
  // exact either way; GS page swizzle is not modeled, so on PS2 treat the
  // map as a budget, not a picture of eDRAM.
  std::size_t vram_bytes = 0;
  int vram_width_px = 0;  // in 16bpp units
  int vram_height_px = 0; // in 16bpp units

  std::size_t main_ram_bytes = 0;

  // Hardware texture limits. A texture wider/taller than this cannot be
  // uploaded as one piece on the console, whatever VRAM is free.
  int max_texture_width = 0;
  int max_texture_height = 0;

  // CLUT (palette) rows a paletted texture needs in VRAM. 0 on PS2, where
  // palettes are handled differently enough that charging for them would
  // mislead; on PS1 a 4bpp texture needs 16 entries, 8bpp needs 256.
  bool textures_use_clut = false;

  // Bit depth assumed for a texture whose loader did not state one.
  // Desktop art is RGBA8888; neither console stores that in VRAM, so the
  // budget has to assume a conversion. PS1 games overwhelmingly ship 8bpp
  // (or 4bpp) paletted textures; PS2 games typically 32bpp or 8bpp.
  int default_texture_bpp = 8;

  // Soft per-frame primitive ceiling used for the over-budget warning.
  // 0 = no warning. These are "you are having a bad day" numbers, not
  // hardware maxima: a PS1 ordering table + GPU FIFO start hurting well
  // before the theoretical polygon rate.
  std::size_t primitive_budget_per_frame = 0;

  RenderSetup render;
};

inline ConsoleProfile ps1_profile() {
  ConsoleProfile p;
  p.name = "PlayStation 1";
  p.vram_bytes = std::size_t{1} << 20; // 1MB
  p.vram_width_px = 1024;
  p.vram_height_px = 512;
  p.main_ram_bytes = std::size_t{2} << 20; // 2MB
  p.max_texture_width = 256;               // one texture page
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
  p.vram_bytes = std::size_t{4} << 20; // 4MB eDRAM
  p.vram_width_px = 2048;              // 4MB expressed as 16bpp units
  p.vram_height_px = 1024;
  p.main_ram_bytes = std::size_t{32} << 20; // 32MB
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

// Width of a w-pixel texture row expressed in the VRAM grid's 16bpp units,
// rounded up: an 8bpp row packs two pixels per unit, a 32bpp row takes two
// units per pixel, a 4bpp row four pixels per unit.
inline int units_for_row(int width_px, int bpp) {
  if (width_px <= 0 || bpp <= 0)
    return 0;
  const int bits = width_px * bpp;
  return (bits + 15) / 16;
}

// Bytes a w x h texture at bpp occupies in VRAM (row padding included, the
// way the allocator will place it).
inline std::size_t texture_bytes(int width_px, int height_px, int bpp) {
  if (width_px <= 0 || height_px <= 0)
    return 0;
  return static_cast<std::size_t>(units_for_row(width_px, bpp)) *
         static_cast<std::size_t>(height_px) * 2u;
}

// CLUT size in 16bpp units for a paletted texture, 0 for direct-color ones.
inline int clut_units(int bpp) {
  if (bpp == 4)
    return 16;
  if (bpp == 8)
    return 256;
  return 0;
}

} // namespace afterhours::psx
