#include "ui_test_harness.h"
#include <afterhours/src/plugins/ui/settings_presets.h>
using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;
namespace {
struct Quality {
  int scale = 1;
  bool shadows = false;
  float fov = 70.f;
  bool operator==(const Quality &) const = default;
};

const std::vector<SettingsPreset<Quality>> PRESETS = {
    {"Low", Quality{1, false, 70.f}},
    {"High", Quality{2, true, 90.f}},
};

std::string stepper_shown(ImmTestHarness &h) {
  std::string shown;
  for (const auto &e : h.coll.get_entities()) {
    if (e && e->has<UIComponentDebug>() &&
        e->get<UIComponentDebug>().name() == "stepper_value" &&
        e->has<HasLabel>()) {
      shown = e->get<HasLabel>().label;
    }
  }
  return shown;
}
}

TEST(select_preset_then_edit_reads_custom) {
  Quality q{1, false, 70.f};
  CHECK(active_preset_name(q, PRESETS) == "Low");
  CHECK(apply_preset(q, PRESETS, 1));
  CHECK(q == (Quality{2, true, 90.f}));
  CHECK(active_preset_name(q, PRESETS) == "High");
  q.fov = 80.f;
  CHECK(active_preset_name(q, PRESETS) == "Custom");
  CHECK(active_preset_index(q, PRESETS) == PRESETS.size());
}

TEST(edit_back_reads_preset_again) {
  Quality q{2, true, 90.f};
  q.fov = 80.f;
  CHECK(active_preset_name(q, PRESETS) == "Custom");
  q.fov = 90.f;
  CHECK(active_preset_name(q, PRESETS) == "High");
  CHECK(active_preset_index(q, PRESETS) == 1);
}

TEST(custom_slot_applies_nothing) {
  Quality q{2, true, 80.f};
  CHECK(!apply_preset(q, PRESETS, PRESETS.size()));
  CHECK(q == (Quality{2, true, 80.f}));
}

TEST(selector_flips_to_custom_and_back) {
  ImmTestHarness h;
  Quality q{1, false, 70.f};
  auto config = ComponentConfig{}
                    .with_size(ComponentSize{pixels(300), pixels(48)})
                    .with_debug_name("preset_stepper");
  settings_preset_stepper(h.context(), mk(h.root(), 0), q, PRESETS, config);
  h.layout_only();
  CHECK(stepper_shown(h) == "Low");
  q.shadows = true;
  h.begin_frame();
  settings_preset_stepper(h.context(), mk(h.root(), 0), q, PRESETS, config);
  h.layout_only();
  CHECK(stepper_shown(h) == "Custom");
  q.shadows = false;
  h.begin_frame();
  settings_preset_stepper(h.context(), mk(h.root(), 0), q, PRESETS, config);
  h.layout_only();
  CHECK(stepper_shown(h) == "Low");
}

int main() { return ui_test::run_registered_tests("settings presets"); }
