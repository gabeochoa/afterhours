#include "ui_test_harness.h"
#include <afterhours/src/plugins/ui/measure_config.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

TEST(tiers_follow_zoom_only_in_adaptive_mode) {
  for (const auto tier : {FontSize::Small, FontSize::Medium, FontSize::Large, FontSize::XL}) {
    const float base = ThemeDefaults::get().theme.font_sizing.get(tier);
    const auto config = ComponentConfig{}.with_font_size(tier);
    for (float height : {600.f, 720.f, 1080.f}) {
      for (float zoom : {0.75f, 1.f, 1.4f, 2.f}) {
        CHECK_APPROX(resolve_to_pixels(config.font_size, height, ScalingMode::Adaptive, zoom), base * zoom);
        CHECK_APPROX(resolve_to_pixels(config.font_size, height, ScalingMode::Proportional, zoom), base * height / 720.f);
        CHECK_APPROX(resolve_to_pixels(h720(base), height, ScalingMode::Adaptive, zoom), base * height / 720.f);
        CHECK_APPROX(resolve_to_pixels(pixels(base), height, ScalingMode::Adaptive, zoom), base * zoom);
        CHECK_APPROX(resolve_to_pixels(pixels(base), height, ScalingMode::Proportional, zoom), base);
      }
    }
  }
}

TEST(tier_sizes_survive_config_copies_and_explicit_replacements) {
  const auto tier = ComponentConfig{}.with_font_size(FontSize::Large);
  const float base = ThemeDefaults::get().theme.font_sizing.get(FontSize::Large);
  const std::array<ComponentConfig, 4> copies{
      ComponentConfig{}.apply_inheritable_from(tier),
      ComponentConfig{}.apply_overrides(tier),
      ComponentConfig{}.with_font("copy", tier.font_size),
      ComponentConfig{}.with_font("copy", FontSize::Large)};
  for (const auto &copy : copies)
    CHECK_APPROX(resolve_to_pixels(copy.font_size, 1080.f, ScalingMode::Adaptive, 1.4f), base * 1.4f);
  auto replaced = tier;
  replaced.with_font_size(h720(18));
  CHECK_APPROX(resolve_to_pixels(replaced.font_size, 1080.f, ScalingMode::Adaptive, 2.f), 27.f);
  replaced = tier;
  replaced.with_font("copy", pixels(18));
  CHECK_APPROX(resolve_to_pixels(replaced.font_size, 1080.f, ScalingMode::Adaptive, 2.f), 36.f);
  const auto merged = tier.apply_overrides(ComponentConfig{}.with_font_size(h720(18)));
  CHECK_APPROX(resolve_to_pixels(merged.font_size, 1080.f, ScalingMode::Adaptive, 2.f), 27.f);
}

static void check_renderer(bool batched, ScalingMode app,
                           std::optional<ScalingMode> screen,
                           std::optional<ScalingMode> component) {
  ui_test::ImmTestHarness h;
  h.begin_frame();
  const auto old_mode = UIStylingDefaults::get().scaling_mode;
  const auto old_scale = ThemeDefaults::get().theme.ui_scale;
  UIStylingDefaults::get().scaling_mode = app;
  ThemeDefaults::get().theme.ui_scale = 1.4f;
  h.context().theme.ui_scale = 1.4f;
  h.context().scaling_mode = screen;
  h.context().screen_height = 1080.f;
  auto config = ComponentConfig{}.with_label("Tier")
      .with_size({pixels(240), pixels(100)})
      .with_font_size(FontSize::Medium).with_text_inset(0);
  if (component) config.with_scaling_mode(*component);
  auto label = div(h.context(), mk(h.root(), 0), config);
  const auto mode = component.value_or(screen.value_or(app));
  const float base = ThemeDefaults::get().theme.font_sizing.get(FontSize::Medium);
  const float expected = base * (mode == ScalingMode::Adaptive ? 1.4f : 1.5f);
  CHECK(label.cmp().resolved_scaling_mode == mode);
  h.coll.merge_entity_arrays();
  std::vector<Entity *> mapping;
  for (const auto &entity : h.coll.get_entities()) {
    if (!entity) continue;
    if (mapping.size() <= static_cast<size_t>(entity->id))
      mapping.resize(static_cast<size_t>(entity->id) + 1);
    mapping[static_cast<size_t>(entity->id)] = entity.get();
  }
  AutoLayout layout({1920, 1080}, mapping);
  layout.ui_scale = 1.4f;
  CHECK_APPROX(layout.get_text_size_for_axis(label.cmp(), Axis::X), expected * 2.f);
  const auto &calls = batched ? h.render_batched() : h.render();
  bool found = false;
  for (const auto &call : calls) {
    if (call.op != "text" || call.text != "Tier") continue;
    CHECK_APPROX(call.rect.height, expected);
    found = true;
  }
  CHECK(found);
  UIStylingDefaults::get().scaling_mode = old_mode;
  ThemeDefaults::get().theme.ui_scale = old_scale;
}

TEST(layout_and_both_renderers_follow_mode_precedence) {
  for (bool batched : {false, true}) {
    check_renderer(batched, ScalingMode::Adaptive, std::nullopt, std::nullopt);
    check_renderer(batched, ScalingMode::Proportional, ScalingMode::Adaptive, std::nullopt);
    check_renderer(batched, ScalingMode::Adaptive, ScalingMode::Proportional, std::nullopt);
    check_renderer(batched, ScalingMode::Adaptive, ScalingMode::Adaptive, ScalingMode::Proportional);
    check_renderer(batched, ScalingMode::Proportional, ScalingMode::Proportional, ScalingMode::Adaptive);
  }
}

TEST(config_measurement_uses_the_selected_font_scaling_mode) {
  ui_test::ImmTestHarness h;
  const auto old_mode = UIStylingDefaults::get().scaling_mode;
  const float old_scale = ThemeDefaults::get().theme.ui_scale;
  UIStylingDefaults::get().scaling_mode = ScalingMode::Adaptive;
  ThemeDefaults::get().theme.ui_scale = 1.4f;
  auto config = ComponentConfig{}.with_label("Tier").with_text_inset(0)
      .with_size({Size{.dim = Dim::Text}, Size{.dim = Dim::Text}}).with_font_size(FontSize::Medium);
  const float base = ThemeDefaults::get().theme.font_sizing.get(FontSize::Medium);
  CHECK_APPROX(measure_config(config, 500).size.y, base * 1.4f);
  config.with_scaling_mode(ScalingMode::Proportional);
  CHECK_APPROX(measure_config(config, 500).size.y, base * 600.f / 720.f);
  UIStylingDefaults::get().scaling_mode = old_mode;
  ThemeDefaults::get().theme.ui_scale = old_scale;
}

int main() { return ui_test::run_registered_tests("font tier scaling"); }
