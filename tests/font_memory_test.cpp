#define FMT_HEADER_ONLY
#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_VULKAN) && !defined(AFTER_HOURS_USE_SOKOL)
#define AFTER_HOURS_USE_SOKOL
#endif
#include <fmt/format.h>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
#define FONTSTASH_IMPLEMENTATION
#include <fontstash/fontstash.h>
#undef FONTSTASH_IMPLEMENTATION
#include <sokol/sokol_gfx.h>
#include <afterhours/src/backends/sokol/font_helper.h>

extern "C" float sapp_dpi_scale(void) { return 1.f; }
static int failures = 0;
#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); ++failures; } } while (0)

int main() {
  namespace md = afterhours::graphics::metal_detail;
  FONSparams params{};
  params.width = params.height = 512;
  params.flags = FONS_ZERO_TOPLEFT;
  md::g_fons_ctx = fonsCreateInternal(&params);
  const auto path = (std::filesystem::path(AFTERHOURS_TEST_FIXTURES) / "AtkinsonHyperlegible-Regular.ttf").string();

  std::vector<unsigned char> bytes;
  CHECK(afterhours::font_coverage::read_file(path.c_str(), bytes));
  const afterhours::Font from_memory = afterhours::load_font_from_memory(bytes.data(), static_cast<int>(bytes.size()), "embedded");
  bytes.assign(bytes.size(), 0);
  CHECK(from_memory.id != FONS_INVALID);
  CHECK(afterhours::font_has_glyph(from_memory, 'A'));
  CHECK(!afterhours::font_has_glyph(from_memory, 0x2603));
  CHECK(afterhours::measure_text(from_memory, "Hello", 24.f, 1.f).x > 0.f);

  CHECK(afterhours::load_font_from_memory(bytes.data(), static_cast<int>(bytes.size()), "embedded").id == from_memory.id);

  const afterhours::Font from_file = afterhours::load_font_from_file(path.c_str());
  CHECK(from_file.id != FONS_INVALID && from_file.id != from_memory.id);
  CHECK(afterhours::load_font_from_file(path.c_str()).id == from_file.id);
  CHECK(afterhours::measure_text(from_file, "Hello", 24.f, 1.f).x ==
        afterhours::measure_text(from_memory, "Hello", 24.f, 1.f).x);
  CHECK(afterhours::load_font_from_file("/nonexistent/font.ttf").id == FONS_INVALID);

  fonsDeleteInternal(md::g_fons_ctx);
  md::g_fons_ctx = nullptr;
  if (failures) {
    std::fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  std::printf("font_memory_test: all checks passed\n");
  return 0;
}
