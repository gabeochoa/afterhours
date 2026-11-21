#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
constexpr bool close(float a, float b) { return std::abs(a - b) < .01f; }
// Layout resolves Dim::None padding to 0 too (see the div computed test),
// so an empty side counts as zero without spelling pixels(0).
constexpr bool resolves_to_zero(const Size &s) {
  return s.dim == Dim::None || close(resolve_to_pixels(s, 720.f), 0.f);
}
} // namespace

// Explicit empty padding must read as zero, while unset must keep taking
// the widget default. Padding{} and "never set" are the same struct, so
// only a caller-set flag can tell them apart.

TEST(explicit_empty_padding_beats_registered_default) {
  auto &defaults = UIStylingDefaults::get();
  const auto saved = defaults.component_configs;
  defaults.set_component_config(ComponentType::Button,
                                ComponentConfig{}.with_padding(
                                    Padding::all(pixels(24.f))));
  const auto merged = defaults.merge_with_defaults(
      ComponentType::Button, ComponentConfig{}.with_padding(Padding{}));
  defaults.component_configs = saved;
  CHECK(resolves_to_zero(merged.padding.top));
  CHECK(resolves_to_zero(merged.padding.left));
  CHECK(resolves_to_zero(merged.padding.bottom));
  CHECK(resolves_to_zero(merged.padding.right));
}

TEST(explicit_zero_padding_beats_registered_default) {
  auto &defaults = UIStylingDefaults::get();
  const auto saved = defaults.component_configs;
  defaults.set_component_config(ComponentType::Button,
                                ComponentConfig{}.with_padding(
                                    Padding::all(pixels(24.f))));
  const auto merged = defaults.merge_with_defaults(
      ComponentType::Button,
      ComponentConfig{}.with_padding(Padding::all(pixels(0.f))));
  defaults.component_configs = saved;
  CHECK(resolves_to_zero(merged.padding.top));
  CHECK(resolves_to_zero(merged.padding.left));
}

TEST(explicit_padding_value_replaces_registered_default) {
  auto &defaults = UIStylingDefaults::get();
  const auto saved = defaults.component_configs;
  defaults.set_component_config(ComponentType::Button,
                                ComponentConfig{}.with_padding(
                                    Padding::all(pixels(24.f))));
  const auto merged = defaults.merge_with_defaults(
      ComponentType::Button,
      ComponentConfig{}.with_padding(Padding::all(pixels(8.f))));
  defaults.component_configs = saved;
  CHECK(close(resolve_to_pixels(merged.padding.top, 720.f), 8.f));
  CHECK(close(resolve_to_pixels(merged.padding.left, 720.f), 8.f));
}

TEST(unset_padding_keeps_registered_default) {
  auto &defaults = UIStylingDefaults::get();
  const auto saved = defaults.component_configs;
  defaults.set_component_config(ComponentType::Button,
                                ComponentConfig{}.with_padding(
                                    Padding::all(pixels(24.f))));
  const auto merged =
      defaults.merge_with_defaults(ComponentType::Button, ComponentConfig{});
  defaults.component_configs = saved;
  CHECK(close(resolve_to_pixels(merged.padding.top, 720.f), 24.f));
  CHECK(close(resolve_to_pixels(merged.padding.left, 720.f), 24.f));
}

TEST(explicit_empty_padding_overrides_base_via_apply_overrides) {
  const ComponentConfig base =
      ComponentConfig{}.with_padding(Padding::all(pixels(24.f)));
  const ComponentConfig merged =
      base.apply_overrides(ComponentConfig{}.with_padding(Padding{}));
  CHECK(resolves_to_zero(merged.padding.top));
  CHECK(merged.padding.top.dim == Dim::None);
}

TEST(unset_override_keeps_base_padding) {
  const ComponentConfig base =
      ComponentConfig{}.with_padding(Padding::all(pixels(24.f)));
  const ComponentConfig merged = base.apply_overrides(ComponentConfig{});
  CHECK(close(resolve_to_pixels(merged.padding.top, 720.f), 24.f));
}

TEST(unset_button_gets_default_padding) {
  ImmTestHarness h;
  h.begin_frame();
  ComponentConfig config;
  afterhours::ui::imm::detail::overwrite_defaults(h.context(), config, ComponentType::Button);
  CHECK(close(resolve_to_pixels(config.padding.top, 720.f), 16.f));
  CHECK(close(resolve_to_pixels(config.padding.left, 720.f), 16.f));
  CHECK(config.padding_is_default);
}

TEST(explicit_empty_padding_button_stays_zero) {
  ImmTestHarness h;
  h.begin_frame();
  ComponentConfig config = ComponentConfig{}.with_padding(Padding{});
  afterhours::ui::imm::detail::overwrite_defaults(h.context(), config, ComponentType::Button);
  CHECK(resolves_to_zero(config.padding.top));
  CHECK(resolves_to_zero(config.padding.left));
  CHECK(resolves_to_zero(config.padding.bottom));
  CHECK(resolves_to_zero(config.padding.right));
  CHECK(config.padding_explicitly_set);
  CHECK(!config.padding_is_default);
}

TEST(explicit_zero_padding_button_stays_zero) {
  ImmTestHarness h;
  h.begin_frame();
  ComponentConfig config =
      ComponentConfig{}.with_padding(Padding::all(pixels(0.f)));
  afterhours::ui::imm::detail::overwrite_defaults(h.context(), config, ComponentType::Button);
  CHECK(close(resolve_to_pixels(config.padding.top, 720.f), 0.f));
  CHECK(!config.padding_is_default);
}

TEST(explicit_empty_padding_on_div_computes_zero) {
  ImmTestHarness h;
  h.begin_frame();
  auto box = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}
                     .with_size({pixels(200), pixels(160)})
                     .with_padding(Padding{}));
  auto child = div(h.context(), mk(box.ent(), 0),
                   ComponentConfig{}.with_size({pixels(20), pixels(20)}));
  h.layout_only();
  for (const Axis edge : {Axis::left, Axis::right, Axis::top, Axis::bottom})
    CHECK(close(box.cmp().computed_padd[edge], 0.f));
  CHECK(close(child.cmp().rect().x - box.cmp().rect().x, 0.f));
  CHECK(close(child.cmp().rect().y - box.cmp().rect().y, 0.f));
}

TEST(unset_padding_on_div_computes_zero) {
  ImmTestHarness h;
  h.begin_frame();
  auto box = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}.with_size({pixels(200), pixels(160)}));
  auto child = div(h.context(), mk(box.ent(), 0),
                   ComponentConfig{}.with_size({pixels(20), pixels(20)}));
  h.layout_only();
  for (const Axis edge : {Axis::left, Axis::right, Axis::top, Axis::bottom})
    CHECK(close(box.cmp().computed_padd[edge], 0.f));
  CHECK(close(child.cmp().rect().x - box.cmp().rect().x, 0.f));
}

int main() { return ui_test::run_registered_tests("padding empty"); }
