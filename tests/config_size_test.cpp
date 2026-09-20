// config_size_test.cpp
// apply_overrides forwards a hand-written field list, so a field added to
// ComponentConfig is silently dropped until someone updates that list. 43 of
// 86 were, long enough that a downstream slide-in did nothing.
//
// This guards the list by size. It lives in a test rather than as a
// static_assert in the header because sizeof is ABI-dependent: the same assert
// in component_config.h broke this test suite, which builds with different
// flags than the app does. A test can be updated by whoever changes the
// struct; a header assert breaks every consumer's build.
//
// If this fails: add your new field to apply_overrides, then update the number.

#include "ui_test_harness.h"

using namespace afterhours::ui::imm;

// Measured in this suite's build config; see the note above about ABI.
static constexpr size_t kExpectedSize = 1456;

TEST(component_config_has_not_grown_unnoticed) {
  CHECK(sizeof(ComponentConfig) == kExpectedSize);
  if (sizeof(ComponentConfig) != kExpectedSize)
    fprintf(stderr,
            "        sizeof(ComponentConfig) is %zu, expected %zu. Merge the "
            "new field in apply_overrides, then update kExpectedSize.\n",
            sizeof(ComponentConfig), kExpectedSize);
}

int main() { return ui_test::run_registered_tests("config size"); }
