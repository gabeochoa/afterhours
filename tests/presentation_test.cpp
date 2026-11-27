
#include "ui_test_harness.h"

#include <afterhours/src/plugins/presentation.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

TEST(present_gates_input_and_dismiss_restores) {
  ImmTestHarness h;
  auto trigger = button(h.context(), mk(h.root(), 9),
                        ComponentConfig{}.with_label("Open"));
  h.context().set_focus(trigger.ent().id);
  bool open = true;
  auto emit = [&] { return presentation::host(h.context(), mk(h.root(), 0), open); };
  auto screen = emit();
  CHECK(screen);
  auto action = button(h.context(), mk(screen.ent(), 1),
                       ComponentConfig{}.with_label("Resume"));
  CHECK(!h.context().is_input_allowed(trigger.ent().id));
  CHECK(h.context().is_input_allowed(action.ent().id));
  CHECK_APPROX(presentation::presence_of(screen.ent()), 1.f);
  h.context().set_focus(action.ent().id);
  h.layout_only();
  open = false;
  h.begin_frame();
  auto gone = emit();
  CHECK(!gone);
  CHECK(h.context().is_input_allowed(trigger.ent().id));
  CHECK(h.context().focus_id == trigger.ent().id);
  CHECK(screen.ent().has<ShouldHide>());
}

TEST(closed_screen_does_not_gate) {
  ImmTestHarness h;
  auto trigger = button(h.context(), mk(h.root(), 9),
                        ComponentConfig{}.with_label("Open"));
  bool open = false;
  auto screen = presentation::host(h.context(), mk(h.root(), 0), open);
  CHECK(!screen);
  CHECK(h.context().is_input_allowed(trigger.ent().id));
}

int main() { return ui_test::run_registered_tests("presentation tests"); }
