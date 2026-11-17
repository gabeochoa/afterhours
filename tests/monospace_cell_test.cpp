#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/text_measure.h>

#include <limits>
#include <string>

using namespace afterhours;
using namespace afterhours::ui;

static int backend_calls = 0;
static std::string last_backend_text;

static void install_counting_backend() {
#ifdef AFTER_HOURS_BACKEND_NONE
  set_measure_text_fn([](const char *text, float font_size, float) {
    backend_calls++;
    last_backend_text = text ? text : "";
    return Vector2Type{static_cast<float>(last_backend_text.size()) *
                           font_size * 0.5f,
                       font_size};
  });
#endif
}

static FontManager *ensure_font_manager() {
  if (auto *fm = EntityHelper::get_singleton_cmp<FontManager>())
    return fm;
  Entity &entity = EntityHelper::createPermanentEntity();
  auto &fm = entity.addComponent<FontManager>();
  fm.load_font("mono", Font{});
  EntityHelper::registerSingleton<FontManager>(entity);
  return &fm;
}

TEST(no_font_manager_returns_zero) {
  install_counting_backend();
  backend_calls = 0;
  CHECK(afterhours::ui::monospace_cell_advance("mono", 20.f) == 0.f);
  CHECK(backend_calls == 0);
}

TEST(cell_advance_bypasses_text_measure_cache) {
  ui_test::ImmTestHarness h;
  CHECK(ensure_font_manager() != nullptr);
  install_counting_backend();
  auto *cache = EntityHelper::get_singleton_cmp<TextMeasureCache>();
  CHECK(cache != nullptr);
  if (!cache)
    return;
  cache->set_measure_function(
      [](std::string_view, std::string_view, float, float) {
        return Vector2Type{999.f, 999.f};
      });
  cache->clear();
  backend_calls = 0;
  last_backend_text.clear();
  const float cell = afterhours::ui::monospace_cell_advance("mono", 20.f);
  CHECK(cell == 10.f);
  CHECK(backend_calls == 1);
  CHECK(last_backend_text == "0");
  CHECK(cache->size() == 0);
  CHECK(cache->misses() == 0);
  CHECK(afterhours::ui::monospace_cell_advance("mono", 40.f) == 20.f);
}

TEST(cell_advance_scales_and_rejects_bad_sizes) {
  ui_test::ImmTestHarness h;
  CHECK(ensure_font_manager() != nullptr);
  install_counting_backend();
  backend_calls = 0;
  CHECK(afterhours::ui::monospace_cell_advance("mono", 0.f) == 0.f);
  CHECK(afterhours::ui::monospace_cell_advance("mono", -4.f) == 0.f);
  CHECK(afterhours::ui::monospace_cell_advance(
            "mono", std::numeric_limits<float>::quiet_NaN()) == 0.f);
  CHECK(backend_calls == 0);
  CHECK(afterhours::monospace_cell_advance(Font{}, 20.f) == 10.f);
}

TEST(gutter_width_is_cell_count_times_advance) {
  ui_test::ImmTestHarness h;
  CHECK(ensure_font_manager() != nullptr);
  install_counting_backend();
  const float cell = afterhours::ui::monospace_cell_advance("mono", 20.f);
  CHECK(cell == 10.f);
  CHECK(5.f * cell == afterhours::measure_text(Font{}, "00000", 20.f, 1.f).x);
  CHECK(4.f * cell == 40.f);
}

int main() { return ui_test::run_registered_tests("monospace cell tests"); }
