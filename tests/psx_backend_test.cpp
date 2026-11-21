// psx_backend_test.cpp
// The PSX backend as a backend: built with AFTER_HOURS_USE_PSX, draw calls
// record into the same capture buffer the none backend uses, and every
// texture/render-target/font goes through the simulated console VRAM.
// psx_budget_test.cpp covers the budget math itself.

#define AFTER_HOURS_USE_PSX
#define AFTERHOURS_STB_IMAGE_IMPLEMENTATION
#include <afterhours/ah.h>
#include <afterhours/src/drawing_helpers.h>
#include <afterhours/src/font_helper.h>

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

static int recorded(const char *op) {
  int n = 0;
  for (const auto &c : draw_calls())
    if (c.op == op)
      ++n;
  return n;
}

int main() {
  budget::configure({.console = psx::Console::PS1});
  clear_draw_calls();

  // Draw calls record exactly like the none backend.
  draw_rectangle(RectangleType{1, 2, 30, 40}, Color{255, 0, 0, 255});
  draw_text("hello", 5, 6, 20, Color{255, 255, 255, 255});
  check(recorded("rectangle") == 1, "rectangle recorded");
  check(recorded("text") == 1, "text recorded");

  // ... and charge the primitive budget (text per glyph).
  budget::end_frame();
  check(budget::report().primitives_last_frame == 1 + 5,
        "rect is 1 primitive, 'hello' is 5 glyphs");

  // Textures from raw pixels are budgeted and freed on unload.
  const std::size_t before = budget::report().vram_used_bytes;
  TextureType tex = load_texture_from_rgba(nullptr, 256, 256);
  check(tex.width == 256.f && tex.height == 256.f, "texture reports its size");
  check(tex.budget_id != 0, "texture carries a budget allocation");
  check(budget::report().vram_used_bytes > before,
        "loading a texture consumes VRAM");
  check(budget::report().texture_count == 1, "texture counted");

  draw_texture_rec(tex, RectangleType{0, 0, 256, 256}, Vector2Type{9, 9},
                   Color{255, 255, 255, 255});
  check(recorded("texture") == 1, "sprite draw recorded");

  unload_texture(tex);
  check(tex.budget_id == 0 && tex.width == 0.f, "unload clears the handle");
  check(budget::report().vram_used_bytes == before,
        "unloading returns the VRAM");

  // Render textures are budgeted at the render depth.
  graphics::RenderTextureType rt = load_render_texture(320, 240);
  check(rt.budget_id != 0, "render texture carries a budget allocation");
  check(budget::report().render_target_count == 1, "render target counted");
  unload_render_texture(rt);
  check(budget::report().render_target_count == 0,
        "render target freed on unload");

  // Fonts charge an atlas page.
  const std::size_t font_before = budget::report().texture_count;
  Font font = load_font_from_file("ui.ttf");
  (void)font;
  check(budget::report().texture_count == font_before + 1,
        "loading a font charges one atlas");

  // Text still measures through the none-style estimate / installed fn.
  const Vector2Type m = measure_text(Font{}, "hello", 20.f, 1.f);
  check(m.x == 50.f && m.y == 20.f, "measure_text estimate matches none");

  // Strict mode: an oversized texture comes back empty, like the console
  // refused the upload.
  budget::configure({.console = psx::Console::PS1, .strict = true});
  TextureType wide = load_texture_from_rgba(nullptr, 1024, 128);
  check(wide.width == 0.f && wide.budget_id == 0,
        "strict mode returns an empty texture for 1024-wide on PS1");
  TextureType fits = load_texture_from_rgba(nullptr, 256, 128);
  check(fits.width == 256.f, "strict mode still loads legal textures");
  unload_texture(fits);

  // The same art is legal on PS2.
  budget::configure({.console = psx::Console::PS2, .strict = true});
  TextureType ps2_tex = load_texture_from_rgba(nullptr, 1024, 128);
  check(ps2_tex.width == 1024.f, "1024-wide texture loads under PS2 profile");
  unload_texture(ps2_tex);

  std::printf("psx_backend_test: %d/%d checks passed\n", checks_passed,
              checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
