// label_clear_test.cpp
// An empty label must ERASE the previous frame's text rather than be ignored.
//
// The tree is rebuilt every frame and ids are positional, so a widget whose
// text goes away has to stop rendering it. Treating "" as "caller said
// nothing" left the old string on screen forever, and because the data was
// right and only the pixels were wrong, it reads as a state-machine bug.
//
// mk() hashes its source location, so every frame has to emit from the SAME
// call site or each frame silently builds a different widget and the test
// proves nothing. Hence the shared emit lambda.

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
struct Emitted {
  EntityID id = 0;
  bool has_label = false;
  std::string label;
};

// One call site, driven across frames by changing `text`.
Emitted emit_status(ImmTestHarness &h, const std::string &text) {
  h.begin_frame();
  auto r = button(h.context(), mk(h.root(), 0),
                  ComponentConfig{}.with_label(text).with_debug_name("status"));
  h.layout_only();
  Emitted out;
  out.id = r.ent().id;
  out.has_label = r.ent().has<HasLabel>();
  if (out.has_label)
    out.label = r.ent().get<HasLabel>().label;
  return out;
}
} // namespace

// The reported case: a status cell that goes from a word to nothing.
TEST(empty_label_clears_previous_frame_text) {
  ImmTestHarness h;

  const Emitted first = emit_status(h, "NOW");
  CHECK(first.has_label);
  CHECK(first.label == "NOW");

  const Emitted second = emit_status(h, "");
  // Same widget, or the clear was never exercised.
  CHECK(second.id == first.id);
  CHECK(second.label.empty());
}

// Clearing must not latch: the text has to come back when it is set again.
TEST(label_can_be_set_again_after_being_cleared) {
  ImmTestHarness h;

  const Emitted first = emit_status(h, "NOW");
  const Emitted cleared = emit_status(h, "");
  const Emitted again = emit_status(h, "SET");

  CHECK(cleared.id == first.id);
  CHECK(again.id == first.id);
  CHECK(cleared.label.empty());
  CHECK(again.has_label);
  CHECK(again.label == "SET");
}

// A widget that never had a label must not gain an empty HasLabel: presence of
// the component is what several render paths test for.
TEST(empty_label_does_not_create_the_component) {
  ImmTestHarness h;

  h.begin_frame();
  auto plain = div(h.context(), mk(h.root(), 0),
                   ComponentConfig{}.with_label("").with_debug_name("plain"));
  h.layout_only();
  CHECK(!plain.ent().has<HasLabel>());
}

int main() { return ui_test::run_registered_tests("label clear tests"); }
