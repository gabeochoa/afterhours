// defer_test.cpp
// UIContext::defer exists so a click handler can ask for teardown without
// performing it inline. Swapping screens from a handler frees the systems and
// entities the handler is running inside, which is a use-after-free every app
// hits the first time it wires a menu button.

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

TEST(deferred_work_does_not_run_immediately) {
  ImmTestHarness h;
  int ran = 0;
  h.context().defer([&] { ran++; });
  CHECK(ran == 0); // still queued
  h.context().run_deferred();
  CHECK(ran == 1);
}

TEST(deferred_work_runs_once) {
  ImmTestHarness h;
  int ran = 0;
  h.context().defer([&] { ran++; });
  h.context().run_deferred();
  h.context().run_deferred(); // a second drain has nothing left
  CHECK(ran == 1);
}

// The reentrant case: a deferred callback that defers more work must not
// invalidate the range being walked. run_deferred takes a copy for this.
TEST(a_callback_may_defer_more_work) {
  ImmTestHarness h;
  int outer = 0, inner = 0;
  h.context().defer([&] {
    outer++;
    h.context().defer([&] { inner++; });
  });

  h.context().run_deferred();
  CHECK(outer == 1);
  CHECK(inner == 0); // the nested one waits for the next drain

  h.context().run_deferred();
  CHECK(inner == 1);
}

TEST(order_is_preserved) {
  ImmTestHarness h;
  std::string order;
  h.context().defer([&] { order += "a"; });
  h.context().defer([&] { order += "b"; });
  h.context().defer([&] { order += "c"; });
  h.context().run_deferred();
  CHECK(order == "abc");
}

int main() { return ui_test::run_registered_tests("defer"); }
