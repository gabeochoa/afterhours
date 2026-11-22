// reset() must return the focus state to pristine, not just unfocused.
//
// The focus ring is drawn off has_interacted and focus_visible() reads
// focus_modality (systems.h), so a reset that keeps them makes the first
// capture after a batch reset draw (or suppress) a ring a single-script run
// would not. Interact, reset, and the context must read like a fresh one.

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using ui_test::ImmTestHarness;

TEST(reset_clears_a_deliberate_focus_move) {
  ImmTestHarness h;
  h.context().set_focus(9, FocusSource::Explicit);
  CHECK(h.context().has_interacted);

  h.context().reset();
  CHECK(!h.context().has_interacted);
  CHECK(!h.context().has_focus(9));
}

TEST(reset_restores_the_fresh_modality) {
  ImmTestHarness h;
  h.context().set_focus(3, FocusSource::Pointer);
  CHECK(!h.context().focus_visible());

  h.context().reset();
  // A fresh context gates the ring on Grab, which is focus-visible.
  CHECK(h.context().focus_visible());
}

TEST(after_reset_the_ring_needs_new_interaction) {
  ImmTestHarness h;
  h.context().set_focus(9, FocusSource::Explicit);
  h.context().reset();

  // The per-frame re-grab gives focus but is not intent, same as fresh.
  h.context().try_to_grab(7);
  CHECK(h.context().has_focus(7));
  CHECK(!h.context().has_interacted);
}

TEST(reset_then_interact_looks_like_fresh_then_interact) {
  ImmTestHarness used;
  used.context().set_focus(9, FocusSource::Explicit);
  used.context().reset();
  used.context().set_focus(9, FocusSource::Explicit);

  ImmTestHarness fresh;
  fresh.context().set_focus(9, FocusSource::Explicit);

  CHECK(used.context().has_interacted == fresh.context().has_interacted);
  CHECK(used.context().focus_visible() == fresh.context().focus_visible());
  CHECK(used.context().has_focus(9) == fresh.context().has_focus(9));
}

int main() { return ui_test::run_registered_tests("reset_focus_test"); }
