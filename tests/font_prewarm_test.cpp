#define FMT_HEADER_ONLY
#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_VULKAN) && !defined(AFTER_HOURS_USE_SOKOL)
#define AFTER_HOURS_USE_SOKOL
#endif
#include <fmt/format.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

static const char *const kText = "AVATAR wide words 0123456789 caf\xc3\xa9";

static FONScontext *context() {
  FONSparams params{};
  params.width = params.height = 2048;
  params.flags = FONS_ZERO_TOPLEFT;
  auto *ctx = fonsCreateInternal(&params);
  CHECK(ctx != nullptr);
  if (!ctx) std::exit(2);
  const auto path = std::filesystem::path(AFTERHOURS_TEST_FIXTURES) / "AtkinsonHyperlegible-Regular.ttf";
  CHECK(fonsAddFont(ctx, "test", path.c_str()) == 0);
  return ctx;
}

static void wire(FONScontext *ctx) {
  afterhours::graphics::metal_detail::g_fons_ctx = ctx;
  afterhours::graphics::metal_detail::g_active_font = 0;
}

static void unwire() {
  afterhours::graphics::metal_detail::g_fons_ctx = nullptr;
  afterhours::graphics::metal_detail::g_active_font = FONS_INVALID;
}

// The baseline first-draw cost: a real draw rasterizes its glyphs.
static int first_draw_rasterizes() {
  auto *ctx = context();
  wire(ctx);
  fonsSetFont(ctx, 0);
  fonsSetSize(ctx, 32);
  CHECK(ctx->fonts[0]->nglyphs == 0);
  fonsDrawText(ctx, 0, 0, kText, nullptr);
  const int rasterized = ctx->fonts[0]->nglyphs;
  CHECK(rasterized > 0);
  fonsDeleteInternal(ctx);
  unwire();
  return rasterized;
}

// After a prewarm, the first real draw rasterizes nothing new.
static void prewarm_covers_first_draw(int expected) {
  auto *ctx = context();
  wire(ctx);
  CHECK(ctx->fonts[0]->nglyphs == 0);
  afterhours::prewarm_text(afterhours::Font{0}, kText, 32.f);
  // Atlas bytes staged at prewarm: glyphs rasterized, dirty rect non-empty.
  CHECK(ctx->fonts[0]->nglyphs == expected);
  int dirty[4]{};
  CHECK(fonsValidateTexture(ctx, dirty) == 1);
  // First real draw: zero new glyph rasterizations, zero new atlas bytes.
  fonsSetFont(ctx, 0);
  fonsSetSize(ctx, 32);
  fonsDrawText(ctx, 0, 0, kText, nullptr);
  CHECK(ctx->fonts[0]->nglyphs == expected);
  CHECK(fonsValidateTexture(ctx, dirty) == 0);
  fonsDeleteInternal(ctx);
  unwire();
}

// Prewarm rasterizes at the draw's raster size (size * dpi), not size.
static void prewarm_matches_retina_draw() {
  test_dpi = 2;
  auto *ctx = context();
  wire(ctx);
  afterhours::prewarm_text(afterhours::Font{0}, kText, 32.f);
  const int rasterized = ctx->fonts[0]->nglyphs;
  CHECK(rasterized > 0);
  fonsSetFont(ctx, 0);
  fonsSetSize(ctx, 64);
  fonsDrawText(ctx, 0, 0, kText, nullptr);
  CHECK(ctx->fonts[0]->nglyphs == rasterized);
  fonsDeleteInternal(ctx);
  unwire();
  test_dpi = 1;
}

// A size that was not prewarmed still pays its own first draw.
static void unprewarmed_size_still_rasterizes() {
  auto *ctx = context();
  wire(ctx);
  afterhours::prewarm_text(afterhours::Font{0}, kText, 32.f);
  const int rasterized = ctx->fonts[0]->nglyphs;
  CHECK(rasterized > 0);
  fonsSetFont(ctx, 0);
  fonsSetSize(ctx, 48);
  fonsDrawText(ctx, 0, 0, kText, nullptr);
  CHECK(ctx->fonts[0]->nglyphs > rasterized);
  fonsDeleteInternal(ctx);
  unwire();
}

static void guards() {
  afterhours::prewarm_text(afterhours::Font{0}, kText, 32.f); // no context
  CHECK(afterhours::graphics::metal_detail::g_fons_ctx == nullptr);
  auto *ctx = context();
  wire(ctx);
  afterhours::prewarm_text(afterhours::Font{0}, nullptr, 32.f);
  afterhours::prewarm_text(afterhours::Font{0}, kText, 0.f);
  afterhours::prewarm_text(afterhours::Font{0}, kText, -1.f);
  CHECK(ctx->fonts[0]->nglyphs == 0);
  // Invalid font id falls back to the active font, like draw_text_ex.
  afterhours::prewarm_text(afterhours::Font{FONS_INVALID}, "A", 32.f);
  CHECK(ctx->fonts[0]->nglyphs > 0);
  fonsDeleteInternal(ctx);
  unwire();
}

int main() {
  const int rasterized = first_draw_rasterizes();
  std::printf("first draw: %d glyphs rasterized; after prewarm: 0 on first draw\n", rasterized);
  prewarm_covers_first_draw(rasterized);
  prewarm_matches_retina_draw();
  unprewarmed_size_still_rasterizes();
  guards();
  std::printf("font prewarm: %d failures\n", failures);
  return failures ? 1 : 0;
}
