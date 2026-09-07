// blend_mode_test.cpp
// The enum is not the interesting part. These pin the two things that save
// time: setting the mode already active does nothing, and a frame can be asked
// how many real transitions it paid for.
//
// puzzle measured 3.8ms/frame of batch flushes to issue 0.005ms of blits,
// because every blit was wrapped in its own pair.

#include "ui_test_harness.h"

using namespace afterhours;

TEST(setting_the_active_mode_again_is_not_a_transition) {
  blend::reset_transition_count();
  CHECK(blend::current() == blend::Mode::Alpha);

  set_blend_mode(blend::Mode::Alpha); // already active
  CHECK(blend::transitions_this_frame() == 0);

  set_blend_mode(blend::Mode::Additive);
  CHECK(blend::transitions_this_frame() == 1);

  set_blend_mode(blend::Mode::Additive); // again
  set_blend_mode(blend::Mode::Additive);
  printf("  three sets of one mode = %zu transitions\n",
         blend::transitions_this_frame());
  CHECK(blend::transitions_this_frame() == 1);

  set_blend_mode(blend::Mode::Alpha);
  blend::reset_transition_count();
}

TEST(a_scope_restores_what_was_there) {
  blend::reset_transition_count();
  CHECK(blend::current() == blend::Mode::Alpha);
  {
    blend_scope scope(blend::Mode::Multiplied);
    CHECK(blend::current() == blend::Mode::Multiplied);
  }
  CHECK(blend::current() == blend::Mode::Alpha);
  CHECK(blend::transitions_this_frame() == 2); // in and out
  blend::reset_transition_count();
}

// The shape that cost puzzle 3.8 ms: N blits, each wrapped in its own scope,
// alternating against surrounding alpha. Two transitions per blit.
TEST(alternating_scopes_cost_two_transitions_each) {
  blend::reset_transition_count();
  for (int i = 0; i < 8; i++) {
    blend_scope scope(blend::Mode::AlphaPremultiply);
    (void)scope;
  }
  printf("  8 alternating blits = %zu transitions\n",
         blend::transitions_this_frame());
  CHECK(blend::transitions_this_frame() == 16);
  blend::reset_transition_count();
}

// And the fix a caller can apply once they can SEE the count: hoist one scope
// around the batch instead of wrapping each blit. Same drawing, 2 transitions
// instead of 16.
TEST(hoisting_one_scope_around_the_batch_costs_two) {
  blend::reset_transition_count();
  {
    blend_scope outer(blend::Mode::AlphaPremultiply);
    for (int i = 0; i < 8; i++) {
      // Nested set to the mode already active: free.
      blend_scope inner(blend::Mode::AlphaPremultiply);
      (void)inner;
    }
  }
  printf("  8 blits under one scope = %zu transitions\n",
         blend::transitions_this_frame());
  CHECK(blend::transitions_this_frame() == 2);
  CHECK(blend::current() == blend::Mode::Alpha);
  blend::reset_transition_count();
}

int main() { return ui_test::run_registered_tests("blend mode"); }
