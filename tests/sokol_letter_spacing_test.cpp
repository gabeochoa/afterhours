#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_VULKAN) && !defined(AFTER_HOURS_USE_SOKOL)
#define AFTER_HOURS_USE_SOKOL
#endif
#include <fmt/format.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#define FONTSTASH_IMPLEMENTATION
#include <fontstash/fontstash.h>
#undef FONTSTASH_IMPLEMENTATION
#include <sokol/sokol_gfx.h>
#include <afterhours/src/backends/sokol/font_helper.h>

static float test_dpi = 1;
extern "C" float sapp_dpi_scale(void) { return test_dpi; }
static int failures = 0;
#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); ++failures; } } while (0)
#define CHECK_NEAR(a, b, eps) do { if (std::fabs(static_cast<double>(a) - static_cast<double>(b)) > static_cast<double>(eps)) { std::fprintf(stderr, "FAIL line %d: %s (%.6f) != %s (%.6f)\n", __LINE__, #a, static_cast<double>(a), #b, static_cast<double>(b)); ++failures; } } while (0)

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

static float rendered_advance(FONScontext *ctx, const char *text, float size,
                              float dpi, float fons_spacing) {
  fonsSetFont(ctx, 0);
  fonsSetSize(ctx, size * dpi);
  fonsSetSpacing(ctx, fons_spacing);
  return fonsTextAdvance(ctx, text, nullptr) / dpi;
}

static void letter_spacing() {
  namespace md = afterhours::graphics::metal_detail;
  auto *ctx = context(2048);
  md::g_fons_ctx = ctx;
  md::g_active_font = 0;
  const float size = 32.f;
  const char *text = "AVATAR";
  for (float dpi : {1.f, 2.f}) {
    test_dpi = dpi;
    afterhours::measure_memo::clear();
    const float base =
        afterhours::measure_text(afterhours::Font{0}, text, size, 1.f).x;
    CHECK(base > 0.f);
    CHECK_NEAR(rendered_advance(ctx, text, size, dpi, 0.f), base, 1e-4f);
    for (float extra : {2.f, 4.f, -0.75f}) {
      afterhours::measure_memo::clear();
      const float m =
          afterhours::measure_text(afterhours::Font{0}, text, size, 1.f + extra).x;
      if (extra > 0.f) {
        CHECK(m > base);
        CHECK_NEAR(m, base + 5.f * extra, 5.f);
      }
      CHECK_NEAR(rendered_advance(ctx, text, size, dpi, extra * dpi), m, 1e-4f);
      fonsSetFont(ctx, 0);
      fonsSetSize(ctx, size * dpi);
      fonsSetSpacing(ctx, extra * dpi);
      CHECK_NEAR(fonsTextBounds(ctx, 0, 0, text, nullptr, nullptr) / dpi, m, 1e-4f);
    }
    afterhours::measure_memo::clear();
    const float two = afterhours::measure_text(afterhours::Font{0}, "WA", size, 3.f).x;
    afterhours::measure_memo::clear();
    const float two_base = afterhours::measure_text(afterhours::Font{0}, "WA", size, 1.f).x;
    CHECK(two > two_base);
    CHECK(two - two_base <= 2.5f);
    const float one = [&] {
      afterhours::measure_memo::clear();
      return afterhours::measure_text(afterhours::Font{0}, "A", size, 1.f).x;
    }();
    afterhours::measure_memo::clear();
    CHECK(afterhours::measure_text(afterhours::Font{0}, "A", size, 5.f).x == one);
    afterhours::measure_memo::clear();
    CHECK(afterhours::measure_text(afterhours::Font{0}, "A", size, 1.f).x == one);
    afterhours::measure_memo::clear();
    CHECK(afterhours::measure_text(afterhours::Font{0}, text, size, 1.f).x == base);
    afterhours::measure_memo::clear();
    CHECK(afterhours::measure_text(afterhours::Font{0}, "", size, 3.f).x == 0.f);
    afterhours::measure_memo::clear();
    const float a = afterhours::measure_text(afterhours::Font{0}, text, size, 1.f).x;
    const float b = afterhours::measure_text(afterhours::Font{0}, text, size, 3.f).x;
    CHECK(b > a);
    CHECK(afterhours::measure_text(afterhours::Font{0}, text, size, 1.f).x == a);
  }
  fonsDeleteInternal(ctx);
  md::g_fons_ctx = nullptr;
}

int main() {
  letter_spacing();
  std::printf("sokol letter spacing: %d failures\n", failures);
  return failures ? 1 : 0;
}
