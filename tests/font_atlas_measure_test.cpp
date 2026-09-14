#define FMT_HEADER_ONLY
#define AFTER_HOURS_USE_METAL
#include <fmt/format.h>
#include <cmath>
#include <chrono>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#define FONTSTASH_IMPLEMENTATION
#include <fontstash/fontstash.h>
#undef FONTSTASH_IMPLEMENTATION
#include <afterhours/src/backends/sokol/font_helper.h>
#include <afterhours/src/core/text_cache.h>

static float test_dpi = 1;
extern "C" float sapp_dpi_scale(void) { return test_dpi; }
static int failures = 0;
#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); ++failures; } } while (0)

static FONScontext *context(int side) {
  FONSparams params{};
  params.width = params.height = side;
  params.flags = FONS_ZERO_TOPLEFT;
  auto *ctx = fonsCreateInternal(&params);
  const auto path = std::filesystem::path(AFTERHOURS_TEST_FIXTURES) / "AtkinsonHyperlegible-Regular.ttf";
  CHECK(ctx != nullptr);
  if (!ctx) std::exit(2);
  CHECK(fonsAddFont(ctx, "test", path.c_str()) == 0);
  return ctx;
}

static void parity() {
  namespace md = afterhours::graphics::metal_detail;
  auto *ctx = context(2048);
  md::g_fons_ctx = ctx;
  md::g_active_font = 0;
  const std::vector<std::string> texts{"", "A", "AVATAR", "To WA", "a  b", "1234567890",
                                       "caf\xc3\xa9", "\xf4\x8f\xbf\xbf", "first\nsecond"};
  for (float size : {9.25f, 16.f, 31.75f, 64.f}) {
    for (float dpi : {1.f, 2.f}) {
      test_dpi = dpi;
      for (float spacing : {0.f, 1.75f, -.75f}) {
        fonsResetAtlas(ctx, 2048, 2048);
        fonsSetFont(ctx, 0);
        fonsSetSize(ctx, size * dpi);
        fonsSetSpacing(ctx, spacing);
        afterhours::measure_memo::clear();
        for (const auto &text : texts) {
          const int count = ctx->fonts[0]->nglyphs;
          const float actual = fonsTextAdvance(ctx, text.c_str(), nullptr);
          CHECK(ctx->fonts[0]->nglyphs == count);
          const float expected = fonsTextBounds(ctx, 0, 0, text.c_str(), nullptr, nullptr);
          CHECK(actual == expected);
          CHECK(afterhours::measure_text(afterhours::Font{0}, text.c_str(), size, 1).x == expected / dpi);
        }
      }
    }
  }
  fonsSetSpacing(ctx, 0);
  fonsSetSize(ctx, 32);
  const char *limited = "AVATAR not measured";
  CHECK(fonsTextAdvance(ctx, limited, limited + 6) == fonsTextBounds(ctx, 0, 0, limited, limited + 6, nullptr));
  CHECK(fonsTextAdvance(ctx, nullptr, nullptr) == 0);
  CHECK(fonsTextAdvance(nullptr, limited, nullptr) == 0);
  const auto fallback_path = std::filesystem::path(AFTERHOURS_TEST_FIXTURES) / "ArchivoNarrow-Regular.ttf";
  const int fallback = fonsAddFont(ctx, "fallback", fallback_path.c_str());
  CHECK(fallback == 1);
  if (fallback != 1) std::exit(2);
  CHECK(fonsAddFallbackFont(ctx, 0, fallback));
  int codepoint = 0;
  for (int c = 128; c < 2048; ++c) {
    if (fons__tt_getGlyphIndex(&ctx->fonts[0]->font, c) == 0 &&
        fons__tt_getGlyphIndex(&ctx->fonts[fallback]->font, c) != 0) {
      codepoint = c;
      break;
    }
  }
  CHECK(codepoint != 0);
  const char fallback_text[]{static_cast<char>(0xc0 | (codepoint >> 6)),
                             static_cast<char>(0x80 | (codepoint & 63)), 0};
  const auto actual = fonsTextAdvance(ctx, fallback_text, nullptr);
  CHECK(actual == fonsTextBounds(ctx, 0, 0, fallback_text, nullptr, nullptr));
  fonsSetFont(ctx, fallback);
  CHECK(actual == fonsTextBounds(ctx, 0, 0, fallback_text, nullptr, nullptr));
  test_dpi = 1;
  fonsDeleteInternal(ctx);
  md::g_fons_ctx = nullptr;
  afterhours::measure_memo::clear();
}

static int benchmark() {
  namespace md = afterhours::graphics::metal_detail;
  auto *ctx = context(2048);
  md::g_fons_ctx = ctx;
  md::g_active_font = 0;
  fonsSetFont(ctx, 0);
  fonsSetSize(ctx, 32);
  const char *text = "AVATAR wide words 0123456789 caf\xc3\xa9";
  using Clock = std::chrono::steady_clock;
  double checksum = 0;
  double cold_ns = 0;
  for (int i = 0; i < 200; ++i) {
    fonsResetAtlas(ctx, 2048, 2048);
    afterhours::measure_memo::clear();
    const auto start = Clock::now();
    checksum += afterhours::measure_text(afterhours::Font{0}, text, 32, 1).x;
    cold_ns += std::chrono::duration<double, std::nano>(Clock::now() - start).count();
  }
  fonsTextBounds(ctx, 0, 0, text, nullptr, nullptr);
  std::vector<std::string> strings;
  for (int i = 0; i < 2000; ++i) strings.push_back(std::string(text) + std::to_string(i));
  afterhours::measure_memo::clear();
  auto start = Clock::now();
  for (const auto &str : strings) checksum += afterhours::measure_text(afterhours::Font{0}, str.c_str(), 32, 1).x;
  const double misses = std::chrono::duration<double, std::nano>(Clock::now() - start).count() / strings.size();
  afterhours::measure_text(afterhours::Font{0}, text, 32, 1);
  start = Clock::now();
  for (int i = 0; i < 100000; ++i) checksum += afterhours::measure_text(afterhours::Font{0}, text, 32, 1).x;
  const double hits = std::chrono::duration<double, std::nano>(Clock::now() - start).count() / 100000;
  std::printf("{\"cold_ns\":%.1f,\"uncached_text_ns\":%.1f,\"cache_hit_ns\":%.1f,\"checksum\":%.0f}\n", cold_ns / 200, misses, hits, checksum);
  fonsDeleteInternal(ctx);
  md::g_fons_ctx = nullptr;
  return 0;
}

int main(int argc, char **argv) {
  if (argc == 2 && std::strcmp(argv[1], "--benchmark") == 0) return benchmark();
  namespace md = afterhours::graphics::metal_detail;
  const std::string text = "AVATAR wide words 0123456789 caf\xc3\xa9";
  auto *large = context(2048);
  fonsSetFont(large, 0);
  fonsSetSize(large, 32);
  const float expected = fonsTextBounds(large, 0, 0, text.c_str(), nullptr, nullptr);
  CHECK(expected > 100);
  auto *small = context(32);
  fonsSetFont(small, 0);
  fonsSetSize(small, 32);
  fonsDrawText(small, 0, 0, "A", nullptr);
  int exhausted = 0;
  fonsSetErrorCallback(small, [](void *p, int error, int) {
    if (error == FONS_ATLAS_FULL) ++*static_cast<int *>(p);
  }, &exhausted);
  fonsDrawText(small, 0, 0, text.c_str(), nullptr);
  CHECK(exhausted > 0);
  md::g_fons_ctx = small;
  md::g_active_font = 0;
  afterhours::measure_memo::clear();
  const int errors_before = exhausted;
  const int glyphs_before = small->fonts[0]->nglyphs;
  CHECK(glyphs_before > 0);
  const std::vector<unsigned char> pixels(small->texData, small->texData + 32 * 32);
  const auto measured = afterhours::measure_text(afterhours::Font{0}, text.c_str(), 32, 1);
  CHECK(measured.x == expected);
  CHECK(exhausted == errors_before);
  CHECK(small->fonts[0]->nglyphs == glyphs_before);
  CHECK(std::memcmp(pixels.data(), small->texData, pixels.size()) == 0);
  const auto hits = afterhours::measure_memo::hits();
  CHECK(afterhours::measure_text(afterhours::Font{0}, text.c_str(), 32, 1).x == expected);
  CHECK(afterhours::measure_memo::hits() == hits + 1);
  afterhours::ui::TextMeasureCache cache([](std::string_view value, std::string_view, float size, float spacing) {
    return afterhours::measure_text(afterhours::Font{0}, std::string(value).c_str(), size, spacing);
  });
  CHECK(cache.measure(text, "test", 32).x == expected);
  CHECK(fonsExpandAtlas(small, 2048, 2048));
  CHECK(fonsDrawText(small, 0, 0, text.c_str(), nullptr) == expected);
  CHECK(cache.measure(text, "test", 32).x == expected);
  CHECK(afterhours::measure_text(afterhours::Font{0}, text.c_str(), 32, 1).x == expected);
  fonsDeleteInternal(small);
  fonsDeleteInternal(large);
  md::g_fons_ctx = nullptr;
  parity();
  std::printf("font atlas measurement: %d failures\n", failures);
  return failures ? 1 : 0;
}
