// dialog_test.cpp
// Regression tests for the modal dialog family (confirm / confirm_danger /
// prompt). Locks the two layout bugs fixed during the dialog overhaul:
//   1. message body overlapping the button row (expand() was greedy), and
//   2. the rightmost action button escaping the panel (children()-width +
//      padding rendered wider than its allocated box).
//
// Build (from the afterhours repo root):
//   clang++ -std=c++23 -I.. -Ivendor tests/dialog_test.cpp -o /tmp/t && /tmp/t

#include "ui_test_harness.h"

#include <afterhours/src/plugins/modal.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

// Dialogs need the ModalRoot singleton (modal_impl throws without it). Put it on
// a permanent entity so it survives the per-test UI-collection cleanup.
static void ensure_modal_singleton() {
  if (!EntityHelper::has_singleton<modal::ModalRoot>()) {
    Entity &e = EntityHelper::createPermanentEntity();
    modal::detail::init_singleton(e);
  }
}

// Assert the standard dialog layout invariants: message sits fully above the
// button row (no overlap) and every action button stays inside the panel.
static void check_dialog_layout(ImmTestHarness &h, size_t expected_buttons) {
  UIComponent *panel = h.find("modal");
  UIComponent *msg = h.find("dialog_message");
  UIComponent *row = h.find("dialog_buttons");
  CHECK(panel != nullptr);
  CHECK(msg != nullptr);
  CHECK(row != nullptr);
  if (!panel || !msg || !row)
    return;

  // 1. Message ends at or above the button row (no overlap).
  CHECK(msg->rect().y + msg->rect().height <= row->rect().y + 2.f);

  // 2. Expected number of buttons, each within the panel's right edge.
  CHECK(row->children.size() == expected_buttons);
  float panel_right = panel->rect().x + panel->rect().width;
  for (EntityID cid : row->children) {
    UIComponent &b = AutoLayout::to_cmp_static(cid);
    CHECK(b.rect().x + b.rect().width <= panel_right + 2.f);
  }

  // 3. The button row fits vertically too. Only the horizontal edge was
  // checked before, so a dialog declared shorter than its own content -- which
  // every variant was -- passed while the row hung out of the bottom.
  float panel_bottom = panel->rect().y + panel->rect().height;
  CHECK(row->rect().y + row->rect().height <= panel_bottom + 2.f);
}

TEST(confirm_dialog_message_above_buttons) {
  ImmTestHarness h;
  ensure_modal_singleton();
  bool open = true;
  modal::confirm(h.context(), mk(h.root(), 0), open, "Apply changes?",
                 "This is a reasonably long confirmation message that should "
                 "wrap across several lines inside the dialog panel.",
                 "Apply", "Cancel");
  h.layout_only();
  check_dialog_layout(h, /*expected_buttons=*/2);
}

TEST(confirm_danger_lays_out_two_buttons) {
  ImmTestHarness h;
  ensure_modal_singleton();
  bool open = true;
  modal::confirm_danger(h.context(), mk(h.root(), 0), open, "Delete save?",
                        "This permanently deletes the file and cannot be undone.",
                        "Delete", "Cancel");
  h.layout_only();
  check_dialog_layout(h, /*expected_buttons=*/2);
}

// NOTE: prompt() embeds a text_input, which requires a fuller InputAction enum
// (TextEnd/MenuBack/... for cursor + editing) than the harness's minimal
// TestInputAction. Its layout reuses the same dialog_message/dialog_button
// helpers exercised above, and it is validated visually by the dialog_prompt
// headless showcase, so it isn't unit-tested here.

int main() { return ui_test::run_registered_tests("dialog tests"); }
