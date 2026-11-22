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

// R-286 probe: caller margins and slot fonts must reach the components.
TEST(a_caller_row_margin_is_not_replaced_by_the_built_in) {
  ImmTestHarness h;
  bool value = true;
  setting_row(h.context(), mk(h.root(), 0),
              SettingRowConfig{}.with_label("Sound"), &value,
              ComponentConfig{}
                  .with_margin(Margin{.left = pixels(20)})
                  .with_debug_name("row_margin"));
  h.layout_only();

  UIComponent *row = h.find("row_margin");
  CHECK(row != nullptr);
  if (row) {
    printf("  row desired margins L=%.0f B=%.0f (asked L=20, built-in B=6)\n",
           row->desired_margin[Axis::left].value,
           row->desired_margin[Axis::bottom].value);
    CHECK(row->desired_margin[Axis::left].value == 20.f);
  }
}

TEST(the_built_in_row_margin_still_applies_by_default) {
  ImmTestHarness h;
  bool value = true;
  setting_row(h.context(), mk(h.root(), 1),
              SettingRowConfig{}.with_label("Sound"), &value,
              ComponentConfig{}.with_debug_name("row_margin_default"));
  h.layout_only();

  UIComponent *row = h.find("row_margin_default");
  CHECK(row != nullptr);
  if (row) {
    printf("  default row desired bottom margin %.0f (row_spacing=6)\n",
           row->desired_margin[Axis::bottom].value);
    CHECK(row->desired_margin[Axis::bottom].value == 6.f);
  }
}

TEST(an_outer_font_name_and_size_reach_the_label) {
  ImmTestHarness h;
  bool value = true;
  setting_row(h.context(), mk(h.root(), 2),
              SettingRowConfig{}.with_label("Sound"), &value,
              ComponentConfig{}
                  .with_font("probe-font", pixels(28.f))
                  .with_debug_name("row_font"));
  h.layout_only();

  UIComponent *label = h.find("setting_row_label");
  CHECK(label != nullptr);
  if (label) {
    printf("  label font '%s' %.0f (asked probe-font 28)\n",
           label->font_name.c_str(), label->font_size.value);
    CHECK(label->font_name == "probe-font");
    CHECK(label->font_size.value == 28.f);
  }
}

TEST(a_slot_label_font_reaches_the_label) {
  ImmTestHarness h;
  bool value = true;
  setting_row(h.context(), mk(h.root(), 3),
              SettingRowConfig{}
                  .with_label("Sound")
                  .with_label_config(
                      ComponentConfig{}.with_font("slot-font", pixels(30.f))),
              &value, ComponentConfig{}.with_debug_name("row_slot"));
  h.layout_only();

  UIComponent *label = h.find("setting_row_label");
  CHECK(label != nullptr);
  if (label) {
    printf("  slot label font '%s' %.0f (asked slot-font 30)\n",
           label->font_name.c_str(), label->font_size.value);
    CHECK(label->font_name == "slot-font");
    CHECK(label->font_size.value == 30.f);
  }
}

TEST(a_slot_control_margin_reaches_the_toggle) {
  ImmTestHarness h;
  bool value = true;
  setting_row(h.context(), mk(h.root(), 4),
              SettingRowConfig{}
                  .with_label("Sound")
                  .with_control_type(SettingRowControlType::Toggle)
                  .with_control_config(
                      ComponentConfig{}.with_margin(
                          Margin{.right = pixels(17)})),
              &value, ComponentConfig{}.with_debug_name("row_toggle"));
  h.layout_only();

  UIComponent *toggle = h.find("setting_row_toggle");
  CHECK(toggle != nullptr);
  if (toggle) {
    printf("  toggle desired right margin %.0f (asked 17, built-in 4)\n",
           toggle->desired_margin[Axis::right].value);
    CHECK(toggle->desired_margin[Axis::right].value == 17.f);
  }
}
