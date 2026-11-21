#define FMT_HEADER_ONLY
#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_VULKAN) && \
    !defined(AFTER_HOURS_USE_SOKOL)
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
#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);        \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

int main() {
  namespace md = afterhours::graphics::metal_detail;
  FONSparams params{};
  params.width = params.height = 512;
  params.flags = FONS_ZERO_TOPLEFT;
  md::g_fons_ctx = fonsCreateInternal(&params);
  const auto path = (std::filesystem::path(AFTERHOURS_TEST_FIXTURES) /
                     "AtkinsonHyperlegible-Regular.ttf")
                        .string();
  std::vector<unsigned char> bytes;
  CHECK(afterhours::font_coverage::read_file(path.c_str(), bytes));

  // 20 synthetic faces (same TTF, distinct names): the old cap tracked 16.
  std::vector<afterhours::Font> fonts;
  for (int i = 0; i < 20; i++) {
    auto f = afterhours::load_font_from_memory(
        bytes.data(), static_cast<int>(bytes.size()),
        ("face" + std::to_string(i)).c_str());
    CHECK(f.id != FONS_INVALID);
    fonts.push_back(f);
  }
  CHECK(md::g_font_ids.size() == 20);

  const int victim_id = fonts[3].id;
  afterhours::unload_font(fonts[3]);
  CHECK(fonts[3].id == FONS_INVALID);
  CHECK(md::g_font_ids.size() == 19);
  CHECK(md::g_font_coverage.count(victim_id) == 0);

  // Reload-after-unload re-registers and measures.
  auto reloaded = afterhours::load_font_from_memory(
      bytes.data(), static_cast<int>(bytes.size()), "face3");
  CHECK(reloaded.id == victim_id);
  CHECK(md::g_font_ids.size() == 20);
  CHECK(afterhours::measure_text(reloaded, "Hello", 24.f, 1.f).x > 0.f);

  // Unloading the active font moves active to a live one.
  md::g_active_font = fonts[0].id;
  afterhours::unload_font(fonts[0]);
  CHECK(md::g_active_font != FONS_INVALID);
  CHECK(afterhours::measure_text(afterhours::Font{}, "Hello", 24.f, 1.f).x >
        0.f);

  fonsDeleteInternal(md::g_fons_ctx);
  md::g_fons_ctx = nullptr;
  if (failures) {
    std::fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  std::printf("font_unload_test: all checks passed\n");
  return 0;
}
