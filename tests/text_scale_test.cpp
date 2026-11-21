#include "ui_test_harness.h"
#include <afterhours/src/plugins/ui/measure_config.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

TEST(resolve_font_size_is_identical_at_scale_one) {
  const auto config = ComponentConfig{}.with_font_size(FontSize::Medium);
  const float base = ThemeDefaults::get().theme.font_sizing.get(FontSize::Medium);
  for (float zoom : {0.75f, 1.f, 1.4f, 2.f}) {
    CHECK_APPROX(resolve_font_size(config.font_size, 1080.f,
                                   ScalingMode::Adaptive, zoom, 1.0f),
                 resolve_to_pixels(config.font_size, 1080.f,
                                   ScalingMode::Adaptive, zoom));
    CHECK_APPROX(resolve_font_size(config.font_size, 720.f,
                                   ScalingMode::Proportional, zoom, 1.0f),
                 base);
  }
}

TEST(resolve_font_size_multiplies_by_the_text_scale) {
  const auto config = ComponentConfig{}.with_font_size(FontSize::Medium);
  const float base = ThemeDefaults::get().theme.font_sizing.get(FontSize::Medium);
  // Adaptive: composes with ui_scale.
  CHECK_APPROX(resolve_font_size(config.font_size, 1080.f,
                                 ScalingMode::Adaptive, 1.4f, 1.5f),
               base * 1.4f * 1.5f);
  // Proportional: h720 resolves against the screen, then the scale applies.
  CHECK_APPROX(resolve_font_size(config.font_size, 1080.f,
                                 ScalingMode::Proportional, 1.4f, 1.5f),
               base * 1.5f * 1.5f);
  // Explicit pixels behave the same way.
  CHECK_APPROX(resolve_font_size(pixels(16), 720.f, ScalingMode::Proportional,
                                 1.f, 1.5f),
               24.f);
}

TEST(text_scale_grows_layout_and_drawn_text) {
  ui_test::ImmTestHarness h;
  h.begin_frame();
  const auto old_mode = UIStylingDefaults::get().scaling_mode;
  const auto old_scale = ThemeDefaults::get().theme.ui_scale;
  const float old_text = ThemeDefaults::get().theme.text_scale;
  UIStylingDefaults::get().scaling_mode = ScalingMode::Adaptive;
  ThemeDefaults::get().theme.ui_scale = 1.0f;
  ThemeDefaults::get().theme.text_scale = 1.5f;
  h.context().theme.ui_scale = 1.0f;
  h.context().theme.text_scale = 1.5f;
  h.context().screen_height = 720.f;
  auto label = div(h.context(), mk(h.root(), 0),
                   ComponentConfig{}
                       .with_label("Text")
                       .with_size({pixels(240), pixels(100)})
                       .with_font_size(FontSize::Medium)
                       .with_text_inset(0));
  const float base = ThemeDefaults::get().theme.font_sizing.get(FontSize::Medium);
  h.coll.merge_entity_arrays();
  std::vector<Entity *> mapping;
  for (const auto &entity : h.coll.get_entities()) {
    if (!entity) continue;
    if (mapping.size() <= static_cast<size_t>(entity->id))
      mapping.resize(static_cast<size_t>(entity->id) + 1);
    mapping[static_cast<size_t>(entity->id)] = entity.get();
  }
  AutoLayout layout({1280, 720}, mapping);
  layout.text_scale = 1.5f;
  CHECK_APPROX(layout.get_text_size_for_axis(label.cmp(), Axis::X), base * 1.5f * 2.f);
  bool found = false;
  for (const auto &call : h.render()) {
    if (call.op != "text" || call.text != "Text") continue;
    CHECK_APPROX(call.rect.height, base * 1.5f);
    found = true;
  }
  CHECK(found);
  UIStylingDefaults::get().scaling_mode = old_mode;
  ThemeDefaults::get().theme.ui_scale = old_scale;
  ThemeDefaults::get().theme.text_scale = old_text;
}

TEST(config_measurement_scales_with_text_scale) {
  const auto old_mode = UIStylingDefaults::get().scaling_mode;
  const float old_scale = ThemeDefaults::get().theme.ui_scale;
  const float old_text = ThemeDefaults::get().theme.text_scale;
  UIStylingDefaults::get().scaling_mode = ScalingMode::Adaptive;
  ThemeDefaults::get().theme.ui_scale = 1.0f;
  ThemeDefaults::get().theme.text_scale = 1.5f;
  auto config = ComponentConfig{}
                    .with_label("Text")
                    .with_text_inset(0)
                    .with_size({Size{.dim = Dim::Text}, Size{.dim = Dim::Text}})
                    .with_font_size(FontSize::Medium);
  const float base = ThemeDefaults::get().theme.font_sizing.get(FontSize::Medium);
  CHECK_APPROX(measure_config(config, 500).size.y, base * 1.5f);
  UIStylingDefaults::get().scaling_mode = old_mode;
  ThemeDefaults::get().theme.ui_scale = old_scale;
  ThemeDefaults::get().theme.text_scale = old_text;
}

int main() { return ui_test::run_registered_tests("text scale"); }
