#include "ui_test_harness.h"
#include <afterhours/src/graphics.h>
#include <afterhours/src/plugins/ui/text_stroke.h>
#include <cassert>
#include <cmath>

int main() {
  using namespace afterhours;
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 96;
  config.height = 96;
  assert(graphics::init(config));
  raylib::GlyphInfo glyph{};
  glyph.value = 'A';
  glyph.advanceX = 10;
  glyph.image = raylib::GenImageColor(5, 9, raylib::BLACK);
  for (int y = 0; y < 9; ++y) raylib::ImageDrawPixel(&glyph.image, 2, y, raylib::WHITE);
  raylib::ImageFormat(&glyph.image, raylib::PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);
  raylib::Rectangle rec{0, 0, 5, 9};
  raylib::Font source{};
  source.baseSize = 9;
  source.glyphCount = 1;
  source.glyphs = &glyph;
  source.recs = &rec;
  source.texture = raylib::GetFontDefault().texture;
  const auto outlined = ui::text_stroke::outline_font(source, "AAA", 9, 8);
  assert(outlined.texture.id != 0);
  const auto cached = ui::text_stroke::outline_font(source, "AAA", 9, 8);
  assert(cached.texture.id == outlined.texture.id);
  const auto before = raylib::MeasureTextEx(source, "AAA", 9, 1);
  const auto after = raylib::MeasureTextEx(outlined, "AAA", 9, 1);
  assert(std::abs(before.x - after.x) < .01f);
  graphics::begin_frame();
  graphics::clear_background(Color{0, 0, 0, 255});
  Arena arena(Arena::DEFAULT_CAPACITY);
  ui::RenderCommandBuffer commands(arena);
  ui::FontManager fonts;
  fonts.load_font("stem", source);
  commands.add_text({40, 40, 1, 9}, "A", "stem", 9, {255, 255, 255, 255},
      ui::TextAlignment::Left, 0, -1, ui::TextStroke::with_color({255, 0, 0, 255}, 8));
  ui::BatchedRenderer renderer;
  renderer.render(commands, fonts);
  graphics::end_frame();
  const auto capture = capture_render_texture_rgba(graphics::get_render_texture());
  assert(capture);
  const auto red = [&](int x, int y) { return capture->pixels[(y * capture->width + x) * 4]; };
  assert(red(36, 44) > 200);
  assert(red(44, 44) > 200);
  assert(red(30, 44) == 0);
  assert(red(32, 44) == 0);
  assert(red(33, 33) == 0);
  ui::text_stroke::clear_cache();
  raylib::UnloadImage(glyph.image);
  graphics::shutdown();
}
