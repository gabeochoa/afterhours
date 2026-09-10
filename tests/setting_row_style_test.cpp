// setting_row_style_test.cpp
// setting_row is broadly restyleable through its slot configs, but three
// writes ignored the caller: the font size was set unconditionally, the label
// was pinned to the default face while the icon and stepper honoured
// config.font_name, and the toggle's margin was applied after the override
// merge rather than before.
//
// All three fail silently -- you ask for something and get the built-in -- so
// they are worth a test rather than a look.

#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/setting_row.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

TEST(a_caller_font_size_is_not_replaced_by_the_built_in) {
  ImmTestHarness h;
  bool value = true;
  setting_row(h.context(), mk(h.root(), 0),
              SettingRowConfig{}.with_label("Sound"), &value,
              ComponentConfig{}
                  .with_font_size(pixels(34.f))
                  .with_debug_name("row"));
  h.layout_only();

  UIComponent *label = h.find("setting_row_label");
  CHECK(label != nullptr);
  if (label) {
    printf("  label font %.0f (asked for 34, built-in is 22)\n",
           label->font_size.value);
    CHECK(label->font_size.value > 22.f);
  }
}

// And with no font size asked for, the built-in still applies -- the guard
// must not turn the default off.
TEST(the_built_in_font_size_still_applies_by_default) {
  ImmTestHarness h;
  bool value = true;
  setting_row(h.context(), mk(h.root(), 1),
              SettingRowConfig{}.with_label("Sound"), &value,
              ComponentConfig{}.with_debug_name("row_default"));
  h.layout_only();

  UIComponent *label = h.find("setting_row_label");
  CHECK(label != nullptr);
  if (label) {
    printf("  default label font %.0f\n", label->font_size.value);
    CHECK(label->font_size.value == 22.f);
  }
}

int main() { return ui_test::run_registered_tests("setting row style"); }
