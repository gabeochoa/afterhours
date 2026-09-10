// grid_test.cpp
// Flex gets you a row of things or a column of things. It does not give you a
// column that lines up across rows, so consumers hand every cell the same
// fixed width -- which is exactly the pattern the resolution sweeps kept
// finding broken. wordproc draws its tables with ~100 lines of raw raylib for
// this reason.
//
// So the property worth pinning is alignment: cell (0,c) and cell (2,c) have
// the same x and the same width, whatever is in them.

#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/grid.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
UIComponent *cell_at(ImmTestHarness &h, int r, int c) {
  return h.find(fmt::format("cell_{}_{}", r, c));
}
} // namespace

TEST(columns_line_up_across_rows) {
  ImmTestHarness h;
  auto tbl = grid(h.context(), mk(h.root(), 0),
                  GridConfig{}.with_rows(3).with_cols(4),
                  ComponentConfig{}
                      .with_size(ComponentSize{pixels(400), pixels(96)})
                      .with_debug_name("tbl"));
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 4; c++)
      // Deliberately ragged content: a fixed-width scheme would be fine, an
      // implementation that sizes to content would not line up.
      grid_cell(h.context(), tbl, r, c,
                ComponentConfig{}
                    .with_label(std::string((size_t)(1 + r * 4 + c), 'x'))
                    .with_debug_name(fmt::format("cell_{}_{}", r, c)));
  h.layout_only();

  for (int c = 0; c < 4; c++) {
    UIComponent *top = cell_at(h, 0, c);
    UIComponent *bottom = cell_at(h, 2, c);
    CHECK(top != nullptr && bottom != nullptr);
    if (!top || !bottom)
      continue;
    printf("  col %d: row0 x=%.0f w=%.0f, row2 x=%.0f w=%.0f\n", c,
           top->rect().x, top->rect().width, bottom->rect().x,
           bottom->rect().width);
    CHECK(std::abs(top->rect().x - bottom->rect().x) < 0.5f);
    CHECK(std::abs(top->rect().width - bottom->rect().width) < 0.5f);
  }
}

TEST(explicit_tracks_are_honoured) {
  ImmTestHarness h;
  auto tbl = grid(h.context(), mk(h.root(), 0),
                  GridConfig{}.with_rows(2).with_cols(3).with_col_widths(
                      {pixels(60), pixels(200), pixels(40)}),
                  ComponentConfig{}
                      .with_size(ComponentSize{pixels(300), pixels(64)})
                      .with_debug_name("tbl2"));
  for (int r = 0; r < 2; r++)
    for (int c = 0; c < 3; c++)
      grid_cell(h.context(), tbl, r, c,
                ComponentConfig{}.with_debug_name(
                    fmt::format("cell_{}_{}", r, c)));
  h.layout_only();

  CHECK(cell_at(h, 0, 0) != nullptr);
  if (cell_at(h, 0, 0)) {
    printf("  tracks: %.0f %.0f %.0f\n", cell_at(h, 0, 0)->rect().width,
           cell_at(h, 0, 1)->rect().width, cell_at(h, 0, 2)->rect().width);
    CHECK(std::abs(cell_at(h, 0, 0)->rect().width - 60.f) < 1.f);
    CHECK(std::abs(cell_at(h, 0, 1)->rect().width - 200.f) < 1.f);
    CHECK(std::abs(cell_at(h, 0, 2)->rect().width - 40.f) < 1.f);
  }
}

// A spanning cell must cover its own track plus the ones it hides, or the rest
// of the row stops lining up with every other row.
TEST(a_spanning_cell_covers_the_tracks_it_hides) {
  ImmTestHarness h;
  auto tbl = grid(h.context(), mk(h.root(), 0),
                  GridConfig{}.with_rows(2).with_cols(4),
                  ComponentConfig{}
                      .with_size(ComponentSize{pixels(400), pixels(64)})
                      .with_debug_name("tbl3"));
  // Row 0: one cell spanning two, then two singles.
  grid_cell(h.context(), tbl, 0, 0,
            ComponentConfig{}.with_debug_name("cell_0_0"), 2);
  grid_cell(h.context(), tbl, 0, 2,
            ComponentConfig{}.with_debug_name("cell_0_2"));
  grid_cell(h.context(), tbl, 0, 3,
            ComponentConfig{}.with_debug_name("cell_0_3"));
  for (int c = 0; c < 4; c++)
    grid_cell(h.context(), tbl, 1, c,
              ComponentConfig{}.with_debug_name(fmt::format("cell_1_{}", c)));
  h.layout_only();

  UIComponent *spanned = cell_at(h, 0, 0);
  UIComponent *single = cell_at(h, 1, 0);
  CHECK(spanned != nullptr && single != nullptr);
  if (spanned && single) {
    printf("  span2 = %.0f, single = %.0f\n", spanned->rect().width,
           single->rect().width);
    CHECK(spanned->rect().width > single->rect().width * 1.8f);
  }
  // The cell after the span still starts where row 1's column 2 does.
  UIComponent *after = cell_at(h, 0, 2);
  UIComponent *below = cell_at(h, 1, 2);
  if (after && below)
    CHECK(std::abs(after->rect().x - below->rect().x) < 1.f);
}

TEST(a_grid_with_no_rows_or_cols_builds_nothing) {
  ImmTestHarness h;
  auto tbl = grid(h.context(), mk(h.root(), 0), GridConfig{}.with_rows(0),
                  ComponentConfig{}.with_debug_name("empty"));
  h.layout_only();
  CHECK(!tbl);
}


// Cells plus the gaps between them have to fit the row. minesweeper's board is
// 16 columns in 580px with a 1px gap: 16 cells of 36 plus 15 gaps is 591, and
// the last column falls off. The old hand-tuned code had the same bug and
// worked around it by picking 35 with a comment saying 36 overflowed.
TEST(cells_and_gaps_together_fit_the_row) {
  ImmTestHarness h;
  auto tbl = grid(h.context(), mk(h.root(), 0),
                  GridConfig{}.with_rows(2).with_cols(16).with_gap(pixels(1)),
                  ComponentConfig{}
                      .with_size(ComponentSize{pixels(580), pixels(80)})
                      .with_debug_name("gaps"));
  for (int r = 0; r < 2; r++)
    for (int c = 0; c < 16; c++)
      grid_cell(h.context(), tbl, r, c,
                ComponentConfig{}.with_debug_name(
                    fmt::format("cell_{}_{}", r, c)));
  h.layout_only();

  UIComponent *first = cell_at(h, 0, 0);
  UIComponent *last = cell_at(h, 0, 15);
  CHECK(first != nullptr && last != nullptr);
  if (first && last) {
    const float span = (last->rect().x + last->rect().width) - first->rect().x;
    printf("  16 cells of %.1f + 15 gaps span %.0f (row is 580)\n",
           first->rect().width, span);
    CHECK(span <= 580.5f);

    // The total fitting is not enough: 16 cells of 36 and no gap at all also
    // fits 580. The stride is what says the gap is really there.
    UIComponent *second = cell_at(h, 0, 1);
    CHECK(second != nullptr);
    if (second) {
      const float stride = second->rect().x - first->rect().x;
      printf("  stride %.2f vs cell %.2f (gap should be 1)\n", stride,
             first->rect().width);
      CHECK(std::abs(stride - (first->rect().width + 1.f)) < 0.5f);
    }
  }
}


// The same grid with snapping on, which is how an app runs. A 1px gap has no
// representation on a 4px grid, so it rounds away and the cells butt together
// -- which is why converting minesweeper's board made it render as one slab
// while every measurement said 36x36 in a 580px row.
TEST(a_sub_unit_gap_does_not_survive_grid_snapping) {
  ImmTestHarness h;
  auto tbl = grid(h.context(), mk(h.root(), 0),
                  GridConfig{}.with_rows(2).with_cols(16).with_gap(pixels(1)),
                  ComponentConfig{}
                      .with_size(ComponentSize{pixels(580), pixels(80)})
                      .with_debug_name("snapped"));
  for (int r = 0; r < 2; r++)
    for (int c = 0; c < 16; c++)
      grid_cell(h.context(), tbl, r, c,
                ComponentConfig{}.with_debug_name(
                    fmt::format("cell_{}_{}", r, c)));
  h.layout_only(true, {1280, 720});

  UIComponent *first = cell_at(h, 0, 0);
  UIComponent *second = cell_at(h, 0, 1);
  CHECK(first != nullptr && second != nullptr);
  if (first && second) {
    const float stride = second->rect().x - first->rect().x;
    printf("  snapped: cell %.0f, stride %.0f (gap asked 1, got %.0f)\n",
           first->rect().width, stride, stride - first->rect().width);
  }

  // The vertical gap between rows comes from the container, not the row, and
  // is a separate thing to get wrong.
  UIComponent *below = cell_at(h, 1, 0);
  if (first && below) {
    const float vstride = below->rect().y - first->rect().y;
    printf("  rows: cell h %.0f, stride %.0f\n", first->rect().height,
           vstride);
  }
}

// A grid sized to children holds its rows at the height that was asked for.
// GridLab used to hand-total the container (three h720(30) rows as h720(90)),
// which is 6px short once each row snaps up to 32 -- the solver then squeezed
// every row to 28, under its own row_height. Hand-totalling is the bug; this
// pins that children() gets it right.
TEST(a_grid_sized_to_children_keeps_its_row_height) {
  ImmTestHarness h;
  auto grid_elem = grid(h.context(), mk(h.root(), 0),
                        GridConfig{}.with_rows(3).with_cols(4)
                            .with_row_height(h720(30)),
                        ComponentConfig{}
                            .with_size(ComponentSize{percent(1.f), children()})
                            .with_debug_name("probe_grid"));
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 4; c++)
      grid_cell(h.context(), grid_elem, r, c,
                ComponentConfig{}.with_debug_name(
                    "probe_" + std::to_string(r) + "_" + std::to_string(c)));
  h.layout_only(true, {1280, 720});

  UIComponent *container = h.find("probe_grid");
  UIComponent *cell = h.find("probe_0_1");
  UIComponent *last = h.find("probe_2_3");
  CHECK(container != nullptr && cell != nullptr && last != nullptr);
  if (!container || !cell || !last) return;
  printf("  container h=%.1f, cell h=%.1f\n", container->rect().height,
         cell->rect().height);
  // Never shorter than the row height asked for.
  CHECK(cell->rect().height >= 30.f);
  // And the container actually covers the last row rather than clipping it.
  CHECK(last->rect().y + last->rect().height <=
        container->rect().y + container->rect().height + 0.5f);
}

int main() { return ui_test::run_registered_tests("grid"); }
