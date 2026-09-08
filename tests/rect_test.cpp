// rect_test.cpp
// The arithmetic a render pass does after layout hands back a rect. puzzle
// carried a 224-line geometry header with 613 uses because the library had
// only intersect_rects and a per-backend get_collision_rec.

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::rect;

namespace {
bool same(RectangleType a, RectangleType b) {
  const auto near = [](float x, float y) { return std::abs(x - y) < 0.001f; };
  return near(a.x, b.x) && near(a.y, b.y) && near(a.width, b.width) &&
         near(a.height, b.height);
}
} // namespace

// The field order is the one thing here that can be wrong silently. Ours is
// Padding's {top, left, bottom, right}; puzzle's original is CSS
// {top, right, bottom, left}, so a positional initialiser means something
// different in each. This pins ours so the swap shows up as a failure.
TEST(sides_are_top_left_bottom_right_not_css_order) {
  const RectangleType r{0, 0, 100, 100};
  // 1 off the top, 2 off the left, 3 off the bottom, 4 off the right.
  const auto p = pad(r, Sides{1, 2, 3, 4});
  printf("  pad{1,2,3,4} -> x=%.0f y=%.0f w=%.0f h=%.0f\n", p.x, p.y, p.width,
         p.height);
  CHECK(same(p, RectangleType{2, 1, 94, 96}));

  // Designated initialisers say what they mean whatever the order is.
  CHECK(same(pad(r, Sides{.left = 10}), RectangleType{10, 0, 90, 100}));
  CHECK(same(pad(r, Sides{.right = 10}), RectangleType{0, 0, 90, 100}));
}

TEST(pad_shrinks_and_expand_grows) {
  const RectangleType r{10, 20, 100, 50};
  CHECK(same(pad(r, 5.f), RectangleType{15, 25, 90, 40}));
  CHECK(same(expand(r, 5.f), RectangleType{5, 15, 110, 60}));
  // Round trip.
  CHECK(same(expand(pad(r, 7.f), 7.f), r));
}

// A pad bigger than the rect must not produce a rect that draws inside out.
TEST(pad_clamps_rather_than_going_negative) {
  const RectangleType r{0, 0, 10, 10};
  const auto p = pad(r, 50.f);
  printf("  over-pad -> w=%.0f h=%.0f\n", p.width, p.height);
  CHECK(p.width == 0.f);
  CHECK(p.height == 0.f);
}

TEST(cuts_return_the_pieces_in_spatial_order) {
  const RectangleType r{0, 0, 200, 100};

  auto [top, below] = cut_top(r, 22.f);
  CHECK(same(top, RectangleType{0, 0, 200, 22}));
  CHECK(same(below, RectangleType{0, 22, 200, 78}));

  auto [above, bottom] = cut_bottom(r, 30.f);
  CHECK(same(above, RectangleType{0, 0, 200, 70}));
  CHECK(same(bottom, RectangleType{0, 70, 200, 30}));

  auto [left, rest] = cut_left(r, 60.f);
  CHECK(same(left, RectangleType{0, 0, 60, 100}));
  CHECK(same(rest, RectangleType{60, 0, 140, 100}));

  auto [start, right] = cut_right(r, 60.f);
  CHECK(same(start, RectangleType{0, 0, 140, 100}));
  CHECK(same(right, RectangleType{140, 0, 60, 100}));
}

// The two halves of a cut have to tile the original exactly, or a layout built
// from repeated cuts drifts.
TEST(a_cut_tiles_the_original_with_no_gap_or_overlap) {
  const RectangleType r{7, 13, 199, 101};
  auto [a, b] = cut_left(r, 55.5f);
  CHECK(a.width + b.width == r.width);
  CHECK(a.x == r.x);
  CHECK(b.x == a.x + a.width);
  CHECK(!overlaps(a, b)); // sharing an edge is not overlapping
}

TEST(cutting_more_than_there_is_leaves_nothing) {
  const RectangleType r{0, 0, 40, 40};
  auto [taken, left] = cut_top(r, 500.f);
  CHECK(same(taken, r));
  CHECK(left.height == 0.f);
}

TEST(align_places_a_box_inside) {
  const RectangleType r{0, 0, 100, 100};
  CHECK(same(align_center(r, 40, 20), RectangleType{30, 40, 40, 20}));
  CHECK(same(align_top(r, 40, 20), RectangleType{30, 0, 40, 20}));
  CHECK(same(align_bottom(r, 40, 20), RectangleType{30, 80, 40, 20}));
  CHECK(same(align_left(r, 40, 20), RectangleType{0, 40, 40, 20}));
  CHECK(same(align_right(r, 40, 20), RectangleType{60, 40, 40, 20}));
}

TEST(contains_and_overlaps) {
  const RectangleType r{0, 0, 100, 100};
  CHECK(contains(r, Vector2Type{50, 50}));
  CHECK(!contains(r, Vector2Type{150, 50}));
  CHECK(overlaps(r, RectangleType{50, 50, 100, 100}));
  CHECK(!overlaps(r, RectangleType{100, 0, 10, 10})); // edge to edge
}

// The repro from the gap report, which needed a geometry header to write.
TEST(the_repro_from_the_report) {
  const RectangleType panel{0, 0, 300, 200};
  auto [header, body] = cut_top(panel, 22.f);
  const auto inner = pad(body, 6.f);
  const auto box = align_center(inner, 64, 36);

  printf("  header %.0fx%.0f, box at %.0f,%.0f\n", header.width, header.height,
         box.x, box.y);
  CHECK(same(header, RectangleType{0, 0, 300, 22}));
  CHECK(same(inner, RectangleType{6, 28, 288, 166}));
  CHECK(same(box, RectangleType{118, 93, 64, 36}));
}

int main() { return ui_test::run_registered_tests("rect"); }
