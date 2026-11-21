// folded_range_test.cpp
// A folded range used to be re-derived by every virtualized reader: skip the
// hidden rows, keep one placeholder, and hope selection/scroll indices agree.
// FoldedRows owns that arithmetic; these check the mapping, not the pixels.

#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/folded_range.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

static void check_roundtrip(const FoldedRows &rows) {
  for (size_t v = 0; v < rows.visible_count(); v++) {
    const size_t m = rows.model_index(v);
    CHECK(rows.visible_index(m) == v);
    CHECK(rows.is_fold_row(v) == rows.is_folded_model(m) &&
          (rows.fold_at(v).begin == m || !rows.is_fold_row(v)));
  }
  for (size_t m = 0; m < rows.model_count(); m++) {
    const size_t v = rows.visible_index(m);
    CHECK(v < rows.visible_count());
    if (rows.is_folded_model(m))
      CHECK(rows.is_fold_row(v));
    else
      CHECK(rows.model_index(v) == m);
  }
}

TEST(no_folds_is_identity) {
  FoldedRows rows(50);
  CHECK(rows.visible_count() == 50);
  check_roundtrip(rows);
}

TEST(empty_model) {
  FoldedRows rows(0, {{0, 5}});
  CHECK(rows.visible_count() == 0);
  CHECK(rows.folds().empty());
  CHECK(rows.model_index(0) == 0);
  CHECK(rows.visible_index(0) == 0);
  CHECK(!rows.fold({0, 1}));
}

TEST(fold_at_first_and_last_rows) {
  FoldedRows first(10, {{0, 4}});
  CHECK(first.visible_count() == 7);
  CHECK(first.is_fold_row(0));
  CHECK(first.model_index(0) == 0);
  CHECK(first.model_index(1) == 4);
  CHECK(first.visible_index(9) == 6);
  check_roundtrip(first);

  FoldedRows last(10, {{6, 10}});
  CHECK(last.visible_count() == 7);
  CHECK(last.is_fold_row(6));
  CHECK(last.model_index(6) == 6);
  CHECK(last.visible_index(9) == 6); // hidden tail lands on the placeholder
  check_roundtrip(last);

  FoldedRows whole(10, {{0, 10}});
  CHECK(whole.visible_count() == 1);
  CHECK(whole.is_fold_row(0));
  check_roundtrip(whole);
}

TEST(nested_and_overlapping_folds_merge_adjacent_do_not) {
  FoldedRows rows(50, {{10, 30}, {15, 20}, {25, 40}});
  CHECK(rows.folds().size() == 1);
  CHECK(rows.folds()[0] == FoldRange({10, 40}));
  check_roundtrip(rows);

  FoldedRows adjacent(20, {{0, 5}, {5, 10}});
  CHECK(adjacent.folds().size() == 2);
  CHECK(adjacent.visible_count() == 12);
  CHECK(adjacent.is_fold_row(0));
  CHECK(adjacent.is_fold_row(1));
  check_roundtrip(adjacent);
}

TEST(multiple_folds_roundtrip) {
  FoldedRows rows(50, {{0, 5}, {20, 35}, {49, 50}});
  CHECK(rows.visible_count() == 50 - 5 - 15 - 1 + 3);
  check_roundtrip(rows);
}

TEST(fold_unfold_and_invalid_ranges) {
  FoldedRows rows(20);
  CHECK(!rows.fold({5, 5}));
  CHECK(!rows.fold({20, 25}));
  CHECK(rows.fold({5, 25})); // end clamps to the model
  CHECK(rows.folds()[0] == FoldRange({5, 20}));
  CHECK(!rows.unfold_model(0));
  CHECK(rows.unfold_visible(rows.visible_index(7)));
  CHECK(rows.visible_count() == 20);

  CHECK(rows.fold({2, 6}));
  CHECK(rows.fold({10, 12}));
  CHECK(rows.unfold_model(11));
  CHECK(rows.folds().size() == 1);
  rows.unfold_all();
  CHECK(rows.visible_count() == 20);
}

TEST(model_shrink_clamps_folds) {
  FoldedRows rows(20, {{15, 20}});
  rows.set_model_count(17);
  CHECK(rows.folds()[0] == FoldRange({15, 17}));
  rows.set_model_count(10);
  CHECK(rows.folds().empty());
  check_roundtrip(rows);
}

// With uniform rows the visible offset is visible_index * row_height, so an
// anchor above a fold must not move, one below shifts by exactly the rows the
// placeholder saved, and unfolding restores the original offset.
TEST(scroll_anchor_offsets) {
  const float h = 20.f;
  FoldedRows rows(100);
  const size_t above = 5, inside = 30, below = 60;
  const float below_before = static_cast<float>(rows.visible_index(below)) * h;

  rows.fold({20, 40}); // 20 hidden rows behind 1 placeholder: saves 19 rows
  CHECK(static_cast<float>(rows.visible_index(above)) * h == 100.f);
  CHECK(static_cast<float>(rows.visible_index(inside)) * h == 400.f);
  CHECK(static_cast<float>(rows.visible_index(below)) * h ==
        below_before - 19.f * h);

  rows.unfold_model(inside);
  CHECK(static_cast<float>(rows.visible_index(below)) * h == below_before);
}

TEST(virtual_list_builds_placeholder_not_hidden_rows) {
  ImmTestHarness h;
  FoldedRows rows(30, {{10, 20}});
  std::vector<size_t> built_models;
  int fold_rows = 0;
  virtual_list(
      h.context(), mk(h.root(), 0), rows.visible_count(), 20.f,
      [&](size_t v, Entity &) {
        if (rows.is_fold_row(v))
          fold_rows++;
        else
          built_models.push_back(rows.model_index(v));
      },
      ComponentConfig{}
          .with_size(ComponentSize{pixels(300), pixels(600)})
          .with_debug_name("vl_folded"));
  h.layout_only();

  CHECK(fold_rows == 1);
  CHECK(built_models.size() + 1 == rows.visible_count());
  for (size_t m : built_models)
    CHECK(m < 10 || m >= 20);
}

int main() { return ui_test::run_registered_tests("folded_range"); }
