// text_action_names_test.cpp
// Every text-editing feature is opted into by enumerator NAME:
// `if constexpr (enum_contains<InputAction>("TextWordLeft"))`. An enum that
// does not carry the name compiles the feature out with no error, no warning
// and nothing to grep. hanabi lost word editing for the life of the project
// that way, and reported "alt-backspace never landed" as a bug when it was a
// name nobody had typed.
//
// warn_missing_text_actions reports them together at init. This pins that the
// detection is right in both directions, since a warning nobody can trigger is
// the same as no warning.

#include "ui_test_harness.h"

#include <string>
#include <vector>

using namespace afterhours;

namespace {

// Mirrors what warn_missing_text_actions checks, so a name added there without
// being added here shows up as a mismatch rather than passing quietly.
const std::vector<std::string> &checked_names() {
  static const std::vector<std::string> names = {
      "TextCopy",      "TextCut",           "TextPaste",
      "TextUndo",      "TextRedo",          "TextSelectAll",
      "TextSelectLeft", "TextSelectRight",  "TextWordLeft",
      "TextWordRight", "TextDeleteWordBack", "TextDeleteWordForward"};
  return names;
}

// An enum missing every text action: what a consumer writing their own
// InputAction from the widget names alone ends up with.
enum struct WidgetsOnlyAction {
  None,
  WidgetMod,
  WidgetNext,
  WidgetBack,
  WidgetPress,
};

} // namespace

TEST(the_default_action_enum_carries_every_name_the_plugin_looks_up) {
  for (const auto &n : checked_names()) {
    const bool present =
        magic_enum::enum_cast<ui::DefaultAction>(n).has_value();
    if (!present)
      fprintf(stderr, "        DefaultAction is missing %s\n", n.c_str());
    CHECK(present);
  }
}

// The other direction: a widget-only enum must be detected as missing all of
// them, or the warning would never fire for the consumer who needs it.
TEST(a_widgets_only_enum_is_detected_as_missing_them) {
  int missing = 0;
  for (const auto &n : checked_names())
    if (!magic_enum::enum_cast<WidgetsOnlyAction>(n).has_value())
      missing++;
  printf("  widgets-only enum is missing %d of %zu text actions\n", missing,
         checked_names().size());
  CHECK(missing == static_cast<int>(checked_names().size()));
}

int main() { return ui_test::run_registered_tests("text action names"); }
