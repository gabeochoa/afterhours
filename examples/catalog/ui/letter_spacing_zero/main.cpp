#include <cstdio>

#include "../../../../ah.h"
#include "../../../../src/plugins/autolayout.h"
#define AFTER_HOURS_IMM_UI
#include "../../../../src/plugins/ui.h"

using namespace afterhours::ui::imm;

int main() {
  const ComponentConfig inherited = ComponentConfig{}.with_letter_spacing(4.f);

  const ComponentConfig cleared =
      inherited.apply_overrides(ComponentConfig{}.with_letter_spacing(0.f));
  const ComponentConfig kept =
      inherited.apply_overrides(ComponentConfig{});
  const ComponentConfig replaced =
      inherited.apply_overrides(ComponentConfig{}.with_letter_spacing(2.f));
  const ComponentConfig plain = ComponentConfig{};

  const bool ok = cleared.letter_spacing.has_value() &&
                  cleared.letter_spacing.value() == 0.f &&
                  kept.letter_spacing.has_value() &&
                  kept.letter_spacing.value() == 4.f &&
                  replaced.letter_spacing.has_value() &&
                  replaced.letter_spacing.value() == 2.f &&
                  !plain.letter_spacing.has_value() &&
                  plain.letter_spacing.value_or(0.f) == 0.f;

  std::printf("letter_spacing_zero: cleared=%.1f kept=%.1f replaced=%.1f "
              "plain=%.1f\n",
              cleared.letter_spacing.value_or(-1.f),
              kept.letter_spacing.value_or(-1.f),
              replaced.letter_spacing.value_or(-1.f),
              plain.letter_spacing.value_or(0.f));
  return ok ? 0 : 1;
}
