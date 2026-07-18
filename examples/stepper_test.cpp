// stepper_test.cpp
// Regression tests for the imm::stepper widget.
//
// Build (from the afterhours repo root):
//   clang++ -std=c++23 -I.. -Ivendor examples/stepper_test.cpp -o /tmp/t && /tmp/t
//
// Covers the multi-visible label separation bug: a stepper showing
// prev/current/next placed its labels in a children()-sized container with
// SpaceAround; with no free space the labels butt together ("Healer" +
// "Warrior" + "Mage" -> "HealerWarriorMage"). The fix adds a gap when
// num_visible > 1; this also exercised the Dim::Children + flex_gap sizing fix
// (a children()-sized container must include gaps in its width).

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

// With num_visible == 3 the label container is wider than the sum of its
// labels (i.e. there is real separation between them).
TEST(stepper_multi_visible_labels_separated) {
  ImmTestHarness h;
  std::vector<std::string> opts = {"Healer", "Warrior", "Mage"};
  size_t idx = 1;

  stepper(h.context(), mk(h.root(), 0), opts, idx,
          ComponentConfig{}
              .with_size(ComponentSize{pixels(400), pixels(56)})
              .with_debug_name("st"),
          /*num_visible=*/3);
  h.layout_and_render();

  UIComponent *labels = h.find("stepper_labels");
  CHECK(labels != nullptr);
  if (labels) {
    float sum_children = 0.f;
    for (EntityID cid : labels->children) {
      auto &c = AutoLayout::to_cmp_static(cid);
      sum_children += c.rect().width;
    }
    // Container is wider than the labels by the inter-label gaps.
    CHECK(labels->rect().width > sum_children + 1.f);
    // Three labels are visible (prev/current/next).
    CHECK(labels->children.size() == 3);
  }
}

// Default single-visible stepper shows exactly one value and adds no gap.
TEST(stepper_single_visible_no_gap) {
  ImmTestHarness h;
  std::vector<std::string> opts = {"Low", "Medium", "High"};
  size_t idx = 1;

  stepper(h.context(), mk(h.root(), 0), opts, idx,
          ComponentConfig{}
              .with_size(ComponentSize{pixels(300), pixels(48)})
              .with_debug_name("st"));
  h.layout_and_render();

  UIComponent *labels = h.find("stepper_labels");
  CHECK(labels != nullptr);
  if (labels) {
    CHECK(labels->children.size() == 1); // only the current value
    float sum_children = 0.f;
    for (EntityID cid : labels->children) {
      auto &c = AutoLayout::to_cmp_static(cid);
      sum_children += c.rect().width;
    }
    // No gap for a single label: container width == the one label's width.
    CHECK_APPROX(labels->rect().width, sum_children);
  }
}

int main() { return ui_test::run_registered_tests("stepper tests"); }
