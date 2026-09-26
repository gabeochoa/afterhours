#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/validation_systems.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

static int checks_run = 0;
static int checks_passed = 0;
static void check(bool cond, const char *what) {
  ++checks_run;
  if (cond) {
    ++checks_passed;
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", what);
  }
}

int main() {
  std::printf("Running contrast validation tests...\n\n");
  ImmTestHarness h;
  auto &validation = UIStylingDefaults::get().get_validation_config_mut();
  validation.enforce_contrast_ratio = true;
  validation.highlight_violations = true;
  validation.min_contrast_ratio = 4.5f;

  h.begin_frame();
  auto label = div(h.context(), mk(h.root(), 0),
                   ComponentConfig{}.with_size({pixels(200), pixels(40)})
                       .with_absolute_position(10, 10)
                       .with_label("explicit red on near-black")
                       .with_custom_background(Color{12, 12, 16, 255})
                       .with_custom_text_color(Color{80, 20, 20, 255}));
  h.layout_only();
  Entity &ent = label.ent();
  ent.get<UIComponent>().was_rendered_to_screen = true;

  validation::ValidateComponentContrast validator;
  validator.for_each_with(ent, ent.get<UIComponent>(), ent.get<HasColor>(),
                          ent.get<HasLabel>(), 0.f);
  check(ent.has<ValidationViolation>(),
        "validator judges the explicit text colour the renderer draws, not "
        "the auto colour the background hint would have picked");

  std::printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run)
    return 1;
  std::printf("All checks passed!\n");
  return 0;
}
