#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

TEST(explicit_zero_clears_inherited_spacing) {
  const ComponentConfig base = ComponentConfig{}.with_letter_spacing(4.f);
  const ComponentConfig merged =
      base.apply_overrides(ComponentConfig{}.with_letter_spacing(0.f));
  CHECK(merged.letter_spacing.has_value());
  CHECK(merged.letter_spacing.value() == 0.f);
}

TEST(unset_override_keeps_inherited_spacing) {
  const ComponentConfig base = ComponentConfig{}.with_letter_spacing(4.f);
  const ComponentConfig merged = base.apply_overrides(ComponentConfig{});
  CHECK(merged.letter_spacing.has_value());
  CHECK(merged.letter_spacing.value() == 4.f);
}

TEST(nonzero_override_replaces_inherited_spacing) {
  const ComponentConfig base = ComponentConfig{}.with_letter_spacing(4.f);
  const ComponentConfig merged =
      base.apply_overrides(ComponentConfig{}.with_letter_spacing(2.f));
  CHECK(merged.letter_spacing.has_value());
  CHECK(merged.letter_spacing.value() == 2.f);
}

TEST(default_config_has_no_letter_spacing) {
  const ComponentConfig config;
  CHECK(!config.letter_spacing.has_value());
}

TEST(explicit_zero_reaches_label) {
  const ComponentConfig base = ComponentConfig{}.with_letter_spacing(4.f);
  const ComponentConfig merged = base.apply_overrides(
      ComponentConfig{}.with_letter_spacing(0.f).with_label("Hi"));
  ImmTestHarness h;
  h.begin_frame();
  auto r = div(h.context(), mk(h.root(), 0), merged);
  h.layout_only();
  CHECK(r.ent().has<HasLabel>());
  CHECK(r.ent().get<HasLabel>().letter_spacing == 0.f);
}

TEST(nonzero_spacing_reaches_label) {
  ImmTestHarness h;
  h.begin_frame();
  auto r = div(h.context(), mk(h.root(), 0),
               ComponentConfig{}.with_label("Hi").with_letter_spacing(3.f));
  h.layout_only();
  CHECK(r.ent().has<HasLabel>());
  CHECK(r.ent().get<HasLabel>().letter_spacing == 3.f);
}

int main() { return ui_test::run_registered_tests("letter spacing zero"); }
