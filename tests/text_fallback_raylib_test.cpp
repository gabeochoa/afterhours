// text_fallback_raylib_test.cpp
// Font fallback chains on raylib, which has none of its own: measure_text and
// draw_text_ex share place_glyphs once a string leaves the primary face.
//
// Renders headless (a GL context, no window) and compares pixels: a string
// drawn whole must match the same string drawn in two pieces, the second at
// the pen measurement gives it. Fonts are the two fixtures and the first
// system CJK face. raylib bakes glyphs at load, so each face is loaded with
// the codepoints these strings use.

#define FMT_HEADER_ONLY
#define AFTER_HOURS_USE_RAYLIB
#include <afterhours/src/graphics.h>
#include <afterhours/src/drawing_helpers.h>
#include <afterhours/src/system_fonts.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

using namespace afterhours;

static int checks = 0, failures = 0;
#define CHECK(cond)                                                            \
  do {                                                                         \
    checks++;                                                                  \
    if (!(cond)) {                                                             \
      failures++;                                                              \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
    }                                                                          \
  } while (0)

namespace {
constexpr int W = 512, H = 96;
const char *kMixed = "Hi \xE6\x97\xA5\xE6\x9C\xAC \xE3\x81\xB2"; // Hi 日本 ひ
const char *kLast = "\xE3\x81\xB2";                              // ひ

std::string fixture(const char *name) {
  return (std::filesystem::path(AFTERHOURS_TEST_FIXTURES) / name).string();
}

bool same(const raylib::Font &a, const raylib::Font &b) {
  return font_key(a) == font_key(b);
}

std::vector<unsigned char> frame(const std::vector<std::pair<std::string, float>> &pieces,
                                 const raylib::Font &font) {
  auto &rt = graphics::get_render_texture();
  graphics::begin_frame();
  graphics::clear_background(Color{0, 0, 0, 255});
  for (const auto &[text, x] : pieces)
    draw_text_ex(font, text.c_str(), Vector2Type{x, 20.f}, 32.f, 1.f,
                 Color{255, 255, 255, 255});
  graphics::end_frame();
  auto rgba = capture_render_texture_rgba(rt);
  return rgba ? rgba->pixels : std::vector<unsigned char>{};
}

size_t lit(const std::vector<unsigned char> &px, int x0, int x1) {
  size_t n = 0;
  for (int y = 0; y < H; y++)
    for (int x = x0; x < x1 && x < W; x++)
      n += px[(static_cast<size_t>(y) * W + x) * 4] > 128;
  return n;
}
} // namespace

int main() {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = W;
  config.height = H;
  if (!graphics::init(config)) {
    std::fprintf(stderr, "no headless GL context\n");
    return 2;
  }
  const auto cjk_faces = system_fonts::cjk();
  if (cjk_faces.empty()) {
    std::fprintf(stderr, "no system CJK face; this test needs one\n");
    return 2;
  }
  std::vector<int> cps = default_codepoints();
  for (int c = 0x100; c <= 0x17F; c++)
    cps.push_back(c); // Latin Extended-A: in Archivo and Hiragino, not Atkinson
  for (int c : {0x65E5, 0x672C, 0x3072, 0xFFFD})
    cps.push_back(c);

  const raylib::Font ui = load_font_from_file(
      fixture("AtkinsonHyperlegible-Regular.ttf").c_str(), 32);
  const raylib::Font narrow =
      load_font_face(fixture("ArchivoNarrow-Regular.ttf").c_str(), 0, 32, cps);
  const raylib::Font cjk = load_font_face(cjk_faces[0].path.c_str(),
                                          cjk_faces[0].face_index, 32, cps);
  CHECK(font_is_usable(ui) && font_is_usable(narrow) && font_is_usable(cjk));

  // Coverage per face.
  CHECK(font_has_glyph(ui, 'A') && !font_has_glyph(ui, 0x65E5));
  CHECK(font_has_glyph(cjk, 0x65E5) && !font_has_glyph(narrow, 0x65E5));

  // Ordering: U+0108 is in both fallbacks; the first one in the chain draws.
  const std::string c_hat = "\xC4\x88";
  CHECK(!font_has_glyph(ui, 0x108) && font_has_glyph(narrow, 0x108) &&
        font_has_glyph(cjk, 0x108));
  std::vector<PlacedGlyph> glyphs;
  Vector2Type extent{};
  CHECK(set_font_fallbacks(ui, {narrow, cjk}));
  CHECK(place_glyphs(ui, c_hat.c_str(), 32, 1, glyphs, extent));
  CHECK(glyphs.size() == 1 && same(glyphs[0].font, narrow));
  const float via_narrow = measure_text(ui, c_hat.c_str(), 32, 1).x;
  CHECK(via_narrow == measure_text(narrow, c_hat.c_str(), 32, 1).x);
  CHECK(set_font_fallbacks(ui, {cjk, narrow})); // replaces
  CHECK(place_glyphs(ui, c_hat.c_str(), 32, 1, glyphs, extent));
  CHECK(glyphs.size() == 1 && same(glyphs[0].font, cjk));
  CHECK(measure_text(ui, c_hat.c_str(), 32, 1).x != via_narrow);
  CHECK(font_fallbacks(ui).size() == 2);

  // Text the primary covers keeps raylib's own measurement exactly.
  CHECK(!place_glyphs(ui, "caf\xC3\xA9 AVA", 32, 1, glyphs, extent));
  CHECK(measure_text(ui, "caf\xC3\xA9 AVA", 32, 1).x ==
        raylib::MeasureTextEx(ui, "caf\xC3\xA9 AVA", 32, 1).x);

  // Mixed Latin/CJK: measurement is the placement's extent, CJK from the
  // fallback, on the primary's baseline.
  CHECK(set_font_fallbacks(ui, {narrow, cjk}));
  CHECK(place_glyphs(ui, kMixed, 32, 1, glyphs, extent));
  CHECK(measure_text(ui, kMixed, 32, 1).x == extent.x);
  CHECK(glyphs.size() == 7 && same(glyphs[3].font, cjk) &&
        same(glyphs[0].font, ui));

  // Uncovered emoji: one visible replacement each, measured as drawn.
  CHECK(place_glyphs(ui, "a\xF0\x9F\x98\x80" "b", 32, 1, glyphs, extent));
  const int replacement = font_has_glyph(cjk, 0xFFFD) ? 0xFFFD : '?';
  CHECK(glyphs.size() == 3 && glyphs[1].codepoint == replacement);
  CHECK(place_glyphs(ui,
                     "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D"
                     "\xF0\x9F\x91\xA7",
                     32, 1, glyphs, extent));
  CHECK(glyphs.size() == 1);

  // Drawing: whole versus split at the measured pen of the last glyph.
  const float pen = measure_text(ui, kMixed, 32, 1).x -
                    measure_text(ui, kLast, 32, 1).x;
  const std::string head(kMixed, std::strlen(kMixed) - std::strlen(kLast));
  const auto whole = frame({{kMixed, 10.f}}, ui);
  const auto split = frame({{head, 10.f}, {kLast, 10.f + pen}}, ui);
  CHECK(!whole.empty() && whole.size() == split.size());
  size_t differ = 0;
  for (size_t i = 0; i < whole.size() && i < split.size(); i++)
    differ += whole[i] != split[i];
  CHECK(differ == 0);
  // The CJK glyphs drew ink (they would be blank from the primary).
  const float cjk_x = 10.f + measure_text(ui, "Hi ", 32, 1).x;
  CHECK(lit(whole, static_cast<int>(cjk_x), static_cast<int>(10.f + pen)) > 50);
  std::fprintf(stderr, "  width %.1f, pen %.1f, differing bytes %zu\n",
               measure_text(ui, kMixed, 32, 1).x, pen, differ);

  graphics::shutdown();
  std::fprintf(stderr, "text fallback (raylib): %d/%d checks passed\n",
               checks - failures, checks);
  return failures == 0 ? 0 : 1;
}
