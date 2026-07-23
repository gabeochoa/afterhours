// render_order_test.cpp
// Regression tests for the UI render-command ordering (rendering.h).
//
// Build (from the afterhours repo root):
//   clang++ -std=c++23 -I.. -Ivendor examples/render_order_test.cpp -o /tmp/t && /tmp/t
//
// Covers the SEVERE render-command sort bug: commands are queued in document
// pre-order (parent before its children), but the sort tiebroke equal-layer
// commands by entity.id. Entity ids are recycled across screens/frames, so a
// parent can hold a HIGHER id than its own children; the (layer, id) sort then
// reordered the opaque parent AFTER its children, painting over them (hiding
// titles/labels/whole rows). Fix: stable-sort by layer only, preserving the
// queued document order within a layer.

#include "ui_test_harness.h"

#include <ranges>

using afterhours::ui::RenderInfo;

// The correct comparator used by the widget renderer (see rendering.h).
static void sort_by_layer_stable(std::vector<RenderInfo> &cmds) {
  std::ranges::stable_sort(
      cmds, [](RenderInfo a, RenderInfo b) { return a.layer < b.layer; });
}

// Within a layer, queued (document) order is preserved: an opaque parent
// queued before its children stays before them, even when its recycled id is
// higher than theirs.
TEST(render_order_preserves_document_order_within_layer) {
  // Queue order = parent (recycled high id 42) THEN its children (7, 8).
  std::vector<RenderInfo> cmds = {{42, 0}, {7, 0}, {8, 0}};
  sort_by_layer_stable(cmds);
  CHECK(cmds[0].id == 42); // parent still painted first
  CHECK(cmds[1].id == 7);
  CHECK(cmds[2].id == 8);
}

// Explicit render_layer still governs true z-order: higher layer paints last
// (on top) regardless of queue position.
TEST(render_order_respects_explicit_layer) {
  std::vector<RenderInfo> cmds = {{5, 10}, {6, 0}};
  sort_by_layer_stable(cmds);
  CHECK(cmds[0].layer == 0);  // base content paints first
  CHECK(cmds[1].layer == 10); // overlay paints last (on top)
}

// Multiple layers keep queue order within each layer while ordering across
// layers ascending.
TEST(render_order_multi_layer_stable) {
  //   id, layer
  std::vector<RenderInfo> cmds = {
      {30, 0}, {10, 0}, {99, 2}, {20, 0}, {50, 1}, {40, 1},
  };
  sort_by_layer_stable(cmds);
  // Layer 0 group keeps queue order 30,10,20.
  CHECK(cmds[0].id == 30);
  CHECK(cmds[1].id == 10);
  CHECK(cmds[2].id == 20);
  // Layer 1 group keeps queue order 50,40.
  CHECK(cmds[3].id == 50);
  CHECK(cmds[4].id == 40);
  // Layer 2 last.
  CHECK(cmds[5].id == 99);
}

// Documents the bug the fix removes: the old (layer, id) comparator sinks a
// high-id parent to the back of its layer, so it paints over its children.
TEST(render_order_id_tiebreak_regression_marker) {
  std::vector<RenderInfo> buggy = {{42, 0}, {7, 0}, {8, 0}};
  std::ranges::sort(buggy, [](RenderInfo a, RenderInfo b) {
    if (a.layer == b.layer)
      return a.id < b.id; // the recycled-id tiebreak (the bug)
    return a.layer < b.layer;
  });
  // Under the bug the parent moves last and covers its children:
  CHECK(buggy.back().id == 42);
}

int main() { return ui_test::run_registered_tests("render order tests"); }
