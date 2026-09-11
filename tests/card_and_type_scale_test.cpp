#include "ui_test_harness.h"

#include <cstdio>
#include <string>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s\n", what.c_str());
  }
}

int main() {
  printf("=== card preset and type scale ===\n\n");

  // with_card is the four settings a panel wants, not a new widget.
  {
    const ComponentConfig card = ComponentConfig{}.with_card();
    check(card.color_usage == Theme::Usage::Surface,
          "a card is on Surface, which the theme already reserves for panels");
    check(card.rounded_corners.has_value() && card.rounded_corners->all(), "with all four corners rounded");
    check(card.padding.left.value > 0.f, "and padding on every side");
  }

  // The caller's padding wins over the default.
  {
    const ComponentConfig tight = ComponentConfig{}.with_card(pixels(4.f));
    check(tight.padding.left.value == 4.f, "the pad argument is honoured");
  }

  // A card is a starting point, not a lock -- anything after it still applies.
  {
    const ComponentConfig custom =
        ComponentConfig{}.with_card().disable_rounded_corners();
    check(custom.rounded_corners.has_value() && custom.rounded_corners->none(),
          "a later call still overrides what the preset set");
  }

  // The type-scale lint is off unless asked for, so a codebase mid-adoption
  // is not drowned.
  {
    check(!UIStylingDefaults::get().get_validation_config()
               .enforce_font_size_tiers,
          "the type scale lint is opt-in");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
