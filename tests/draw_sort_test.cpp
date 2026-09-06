// draw_sort_test.cpp
// sort() had no callers, so its type tiebreak had never moved a scissor off
// what it clips. Opt-in now, which only helps if it is also correct.

#include <cstdio>
#include <string>
#include <vector>

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;

namespace {
int checks_run = 0, checks_passed = 0;
void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond)
    checks_passed++;
  else
    fprintf(stderr, "  FAIL: %s\n", what.c_str());
}

// Layers in emission order, for readability in the assertions below.
std::vector<int> layers_of(const RenderCommandBuffer &buf) {
  std::vector<int> out;
  for (const auto &c : buf.commands())
    out.push_back(c.layer);
  return out;
}
} // namespace

int main() {
  printf("=== draw sort tests ===\n\n");

  const RectangleType r{0, 0, 10, 10};
  const Color c{255, 255, 255, 255};

  // Out of layer order on purpose.
  {
    Arena arena(Arena::DEFAULT_CAPACITY);
    RenderCommandBuffer buf(arena);
    buf.add_rectangle(r, c, /*layer=*/5, 1);
    buf.add_rectangle(r, c, /*layer=*/1, 2);
    buf.add_rectangle(r, c, /*layer=*/3, 3);

    check(layers_of(buf) == std::vector<int>({5, 1, 3}),
          "unsorted, the buffer keeps emission order");

    buf.sort();
    check(layers_of(buf) == std::vector<int>({1, 3, 5}),
          "sorted, the buffer is ordered by layer");
  }

  // Same layer: emission order is the paint order, so it must survive.
  {
    Arena arena(Arena::DEFAULT_CAPACITY);
    RenderCommandBuffer buf(arena);
    buf.add_rectangle(r, c, 2, /*entity=*/10);
    buf.add_rectangle(r, c, 2, /*entity=*/11);
    buf.add_rectangle(r, c, 2, /*entity=*/12);
    buf.sort();
    std::vector<int> ids;
    for (const auto &cmd : buf.commands())
      ids.push_back(cmd.entity_id);
    check(ids == std::vector<int>({10, 11, 12}),
          "within one layer the sort is stable");
  }

  // A scissor must still bracket its geometry afterwards.
  {
    Arena arena(Arena::DEFAULT_CAPACITY);
    RenderCommandBuffer buf(arena);
    buf.add_scissor_start(0, 0, 10, 10, 4, 20);
    buf.add_rectangle(r, c, 4, 21);
    buf.add_scissor_end(4, 22);
    buf.sort();

    const auto &cmds = buf.commands();
    check(cmds.size() == 3, "three commands survive the sort");
    if (cmds.size() == 3) {
      check(cmds[0].type == RenderPrimitiveType::ScissorStart &&
                cmds[2].type == RenderPrimitiveType::ScissorEnd,
            "the scissor still wraps the geometry it clips");
    }
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
