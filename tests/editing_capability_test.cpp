// editing_capability_test.cpp
// Editing actions are gated on the consumer's enum carrying a matching name,
// so a name nobody wrote down removes the feature silently. hanabi went
// without word editing for its whole life.

#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/text_input/component.h>

#include <cstdio>
#include <string>

using afterhours::text_input::has_editing_action;
using afterhours::text_input::optional_editing_actions;

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

enum struct FullActions {
  TextUndo,
  TextRedo,
  TextCopy,
  TextCut,
  TextPaste,
  TextWordLeft,
  TextWordRight,
  TextSelectLeft,
  TextSelectRight,
  TextDeleteWordBack,
  TextDeleteWordForward,
};

// Wired the clipboard, never heard of the rest.
enum struct PartialActions {
  TextCopy,
  TextCut,
  TextPaste,
};

int main() {
  printf("=== text editing capability ===\n\n");

  static_assert(has_editing_action<FullActions>("TextWordLeft"));
  static_assert(!has_editing_action<PartialActions>("TextWordLeft"));

  {
    int found = 0;
    for (const auto name : optional_editing_actions)
      if (has_editing_action<FullActions>(name))
        found++;
    printf("  full enum: %d of %zu actions\n", found,
           std::size(optional_editing_actions));
    check(found == (int)std::size(optional_editing_actions),
          "an enum naming every action gets every action");
  }

  {
    int found = 0;
    for (const auto name : optional_editing_actions)
      if (has_editing_action<PartialActions>(name))
        found++;
    printf("  clipboard-only enum: %d of %zu actions\n", found,
           std::size(optional_editing_actions));
    check(found == 3, "and one naming three gets exactly those three");
    check(!has_editing_action<PartialActions>("TextDeleteWordBack"),
          "alt-backspace is off, and now says so");
  }

  // A typo looks the same as opting out, so the warning lists names.
  {
    enum struct TypoActions { TextWordLeft, TextWordRigth };
    check(has_editing_action<TypoActions>("TextWordLeft"),
          "the correctly spelled one resolves");
    check(!has_editing_action<TypoActions>("TextWordRight"),
          "the typo does not, and looks the same as opting out");
  }

  // A dead name here would warn forever about something unsatisfiable.
  {
    bool all_named = true;
    for (const auto name : optional_editing_actions)
      if (name.empty() || name.substr(0, 4) != "Text")
        all_named = false;
    check(all_named, "every listed action is a Text* name");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
