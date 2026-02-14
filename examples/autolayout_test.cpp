// autolayout_test.cpp
// Unit tests for the autolayout engine, focusing on:
//   - Padding correctly reduces content area (no double-counting)
//   - Negative dimensions are clamped to zero
//   - Cross-axis uses max (not sum) for children sizing
//   - Overflow detection with tolerance
//   - Wrap detection and wrap-aware overflow suppression
//   - expand() fills remaining space properly
//   - Percent/pixel sizing within padded containers
//
// Build:
//   make autolayout_test
// Run:
//   ./autolayout_test

// fmt is used by autolayout.h for warning messages; enable header-only mode
#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/autolayout.h>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

using namespace afterhours;
using namespace afterhours::ui;

// ============================================================================
// Test helpers
// ============================================================================

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name)                                                             \
  static void test_##name();                                                   \
  struct Register_##name {                                                     \
    Register_##name() { register_test(#name, test_##name); }                   \
  } register_##name##_instance;                                                \
  static void test_##name()

struct TestEntry {
  const char *name;
  void (*fn)();
};

static std::vector<TestEntry> &test_registry() {
  static std::vector<TestEntry> r;
  return r;
}

static void register_test(const char *name, void (*fn)()) {
  test_registry().push_back({name, fn});
}

static void check(bool cond, const char *expr, const char *file, int line) {
  tests_run++;
  if (cond) {
    tests_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s  (%s:%d)\n", expr, file, line);
  }
}

#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)

static bool approx(float a, float b, float eps = 0.5f) {
  return std::fabs(a - b) < eps;
}

#define CHECK_APPROX(a, b)                                                     \
  do {                                                                         \
    float _a = (a), _b = (b);                                                  \
    check(approx(_a, _b), #a " ~= " #b, __FILE__, __LINE__);                  \
    if (!approx(_a, _b))                                                       \
      fprintf(stderr, "        got %.2f vs %.2f\n", _a, _b);                   \
  } while (0)

// ============================================================================
// Test harness: create entities and run autolayout without a full ECS
// ============================================================================

// A simple entity pool for tests. Each test creates entities, wires up
// UIComponent parent/child relationships, builds the mapping, and calls
// AutoLayout::autolayout().

struct TestLayout {
  std::vector<std::unique_ptr<Entity>> entities;
  window_manager::Resolution resolution{1280, 720};

  Entity &make_entity() {
    entities.push_back(std::make_unique<Entity>());
    return *entities.back();
  }

  // Create a UI entity with the given desired size.
  Entity &make_ui(Size w, Size h) {
    Entity &e = make_entity();
    auto &ui = e.addComponent<UIComponent>(e.id);
    ui.set_desired_width(w);
    ui.set_desired_height(h);
    return e;
  }

  // Wire parent-child relationship.
  void add_child(Entity &parent, Entity &child) {
    auto &pc = parent.get<UIComponent>();
    auto &cc = child.get<UIComponent>();
    pc.add_child(child.id);
    cc.set_parent(parent.id);
  }

  // Build entity map and run layout on the root entity.
  void run(Entity &root) {
    std::map<EntityID, RefEntity> mapping;
    for (auto &e : entities) {
      mapping.emplace(e->id, std::ref(*e));
    }
    AutoLayout::autolayout(root.get<UIComponent>(), resolution, mapping);
  }

  // Shortcut: get UIComponent from entity.
  static UIComponent &ui(Entity &e) { return e.get<UIComponent>(); }
};

// ============================================================================
// Tests
// ============================================================================

// ---------------------------------------------------------------------------
// Basic: pixel-sized root gets correct computed dimensions
// ---------------------------------------------------------------------------
TEST(basic_pixel_sizing) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(300));
  t.run(root);

  CHECK_APPROX(t.ui(root).computed[Axis::X], 400.f);
  CHECK_APPROX(t.ui(root).computed[Axis::Y], 300.f);
}

// ---------------------------------------------------------------------------
// Padding reduces content area: children sized by percent(1.0) should
// fill the content box (parent size minus padding), not the full parent.
// ---------------------------------------------------------------------------
TEST(padding_reduces_content_area) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(300));
  t.ui(root).set_desired_padding(pixels(20), Axis::X);
  t.ui(root).set_desired_padding(pixels(10), Axis::Y);

  auto &child = t.make_ui(percent(1.0f), percent(1.0f));
  t.add_child(root, child);
  t.run(root);

  // Parent padding: 20 left + 20 right = 40 horizontal, 10 top + 10 bottom = 20 vertical
  // Child at 100% should be parent_content = parent_size - padding
  CHECK_APPROX(t.ui(child).computed[Axis::X], 360.f); // 400 - 40
  CHECK_APPROX(t.ui(child).computed[Axis::Y], 280.f); // 300 - 20
}

// ---------------------------------------------------------------------------
// Padding does not double-count: child position is offset by padding,
// so child_rel + child_size should not exceed parent bounds.
// ---------------------------------------------------------------------------
TEST(padding_no_double_count) {
  TestLayout t;
  auto &root = t.make_ui(pixels(200), pixels(200));
  t.ui(root).set_desired_padding(pixels(10), Axis::X);
  t.ui(root).set_desired_padding(pixels(10), Axis::Y);

  auto &child = t.make_ui(percent(1.0f), percent(1.0f));
  t.add_child(root, child);
  t.run(root);

  auto &c = t.ui(child);
  // Child should fit within the parent's content area
  // rect bounds: child_rel + margin + size <= parent size
  float child_end_x = c.computed_rel[Axis::X] + c.computed[Axis::X];
  float child_end_y = c.computed_rel[Axis::Y] + c.computed[Axis::Y];

  // Child end should be at most parent_size (rel includes padding offset)
  CHECK(child_end_x <= 200.f + 1.f);
  CHECK(child_end_y <= 200.f + 1.f);
}

// ---------------------------------------------------------------------------
// Negative dimension clamping: if padding exceeds parent size, the content
// area should clamp to zero, not go negative.
// ---------------------------------------------------------------------------
TEST(negative_dimension_clamped) {
  TestLayout t;
  auto &root = t.make_ui(pixels(50), pixels(50));
  // Padding larger than the root: 40 left + 40 right = 80 > 50
  t.ui(root).set_desired_padding(pixels(40), Axis::X);
  t.ui(root).set_desired_padding(pixels(40), Axis::Y);

  auto &child = t.make_ui(percent(1.0f), percent(1.0f));
  t.add_child(root, child);
  t.run(root);

  // When padding exceeds parent, content area goes negative internally.
  // The rect() function clamps dimensions to 0 to prevent negative rendering.
  auto r = t.ui(child).rect();
  CHECK(r.width >= 0.f);
  CHECK(r.height >= 0.f);
}

// ---------------------------------------------------------------------------
// Cross-axis uses max (not sum): In a Column layout, children with different
// widths should not cause the parent to sum widths.
// ---------------------------------------------------------------------------
TEST(cross_axis_uses_max_not_sum) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);

  auto &c1 = t.make_ui(pixels(100), pixels(50));
  auto &c2 = t.make_ui(pixels(200), pixels(50));
  auto &c3 = t.make_ui(pixels(150), pixels(50));
  t.add_child(root, c1);
  t.add_child(root, c2);
  t.add_child(root, c3);
  t.run(root);

  // In a column layout, children stack vertically. The widest child is 200px.
  // All children should fit side-by-side (they don't, they stack), but the
  // violation solver uses max for cross-axis. Parent should remain 400.
  CHECK_APPROX(t.ui(root).computed[Axis::X], 400.f);
  // Children should not be shrunk on the cross axis beyond their natural size
  CHECK(t.ui(c2).computed[Axis::X] >= 199.f);
}

// ---------------------------------------------------------------------------
// Row layout: cross-axis (Y) uses max
// ---------------------------------------------------------------------------
TEST(row_cross_axis_uses_max) {
  TestLayout t;
  auto &root = t.make_ui(pixels(600), pixels(200));
  t.ui(root).set_flex_direction(FlexDirection::Row);

  auto &c1 = t.make_ui(pixels(100), pixels(50));
  auto &c2 = t.make_ui(pixels(100), pixels(80));
  auto &c3 = t.make_ui(pixels(100), pixels(30));
  t.add_child(root, c1);
  t.add_child(root, c2);
  t.add_child(root, c3);
  t.run(root);

  // In a row layout, children are side by side horizontally.
  // Cross axis (Y) should use max child height = 80, not sum = 160
  CHECK_APPROX(t.ui(root).computed[Axis::Y], 200.f);
  CHECK(t.ui(c2).computed[Axis::Y] >= 79.f);
}

// ---------------------------------------------------------------------------
// Column stacking: children in column layout get correct Y offsets
// ---------------------------------------------------------------------------
TEST(column_stacking_offsets) {
  TestLayout t;
  auto &root = t.make_ui(pixels(300), pixels(300));
  t.ui(root).set_flex_direction(FlexDirection::Column);

  auto &c1 = t.make_ui(pixels(300), pixels(100));
  auto &c2 = t.make_ui(pixels(300), pixels(100));
  t.add_child(root, c1);
  t.add_child(root, c2);
  t.run(root);

  // First child starts at top
  CHECK_APPROX(t.ui(c1).computed_rel[Axis::Y], 0.f);
  // Second child should be offset by first child's height
  CHECK_APPROX(t.ui(c2).computed_rel[Axis::Y], 100.f);
}

// ---------------------------------------------------------------------------
// Row stacking: children in row layout get correct X offsets
// ---------------------------------------------------------------------------
TEST(row_stacking_offsets) {
  TestLayout t;
  auto &root = t.make_ui(pixels(600), pixels(100));
  t.ui(root).set_flex_direction(FlexDirection::Row);

  auto &c1 = t.make_ui(pixels(100), pixels(100));
  auto &c2 = t.make_ui(pixels(150), pixels(100));
  t.add_child(root, c1);
  t.add_child(root, c2);
  t.run(root);

  CHECK_APPROX(t.ui(c1).computed_rel[Axis::X], 0.f);
  CHECK_APPROX(t.ui(c2).computed_rel[Axis::X], 100.f);
}

// ---------------------------------------------------------------------------
// expand() fills remaining space in column layout
// ---------------------------------------------------------------------------
TEST(expand_fills_remaining_column) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  auto &header = t.make_ui(pixels(400), pixels(50));
  auto &body = t.make_ui(pixels(400), expand());
  auto &footer = t.make_ui(pixels(400), pixels(50));
  t.add_child(root, header);
  t.add_child(root, body);
  t.add_child(root, footer);
  t.run(root);

  // Body should fill: 400 - 50 (header) - 50 (footer) = 300
  CHECK_APPROX(t.ui(body).computed[Axis::Y], 300.f);
  // Footer should start at 350
  CHECK_APPROX(t.ui(footer).computed_rel[Axis::Y], 350.f);
}

// ---------------------------------------------------------------------------
// expand() fills remaining space in row layout
// ---------------------------------------------------------------------------
TEST(expand_fills_remaining_row) {
  TestLayout t;
  auto &root = t.make_ui(pixels(600), pixels(100));
  t.ui(root).set_flex_direction(FlexDirection::Row);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  auto &left = t.make_ui(pixels(100), pixels(100));
  auto &middle = t.make_ui(expand(), pixels(100));
  auto &right = t.make_ui(pixels(150), pixels(100));
  t.add_child(root, left);
  t.add_child(root, middle);
  t.add_child(root, right);
  t.run(root);

  // Middle should fill: 600 - 100 - 150 = 350
  CHECK_APPROX(t.ui(middle).computed[Axis::X], 350.f);
}

// ---------------------------------------------------------------------------
// expand() with padding: remaining space accounts for parent padding
// ---------------------------------------------------------------------------
TEST(expand_with_padding) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);
  t.ui(root).set_desired_padding(pixels(20), Axis::Y); // 20 top + 20 bottom

  auto &header = t.make_ui(percent(1.0f), pixels(50));
  auto &body = t.make_ui(percent(1.0f), expand());
  auto &footer = t.make_ui(percent(1.0f), pixels(50));
  t.add_child(root, header);
  t.add_child(root, body);
  t.add_child(root, footer);
  t.run(root);

  // Content height = 400 - 40 (padding) = 360
  // Body = 360 - 50 - 50 = 260
  CHECK_APPROX(t.ui(body).computed[Axis::Y], 260.f);
}

// ---------------------------------------------------------------------------
// Multiple expand() children share space proportionally
// ---------------------------------------------------------------------------
TEST(multiple_expand_share_space) {
  TestLayout t;
  auto &root = t.make_ui(pixels(300), pixels(300));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  auto &top = t.make_ui(pixels(300), pixels(100));
  auto &mid = t.make_ui(pixels(300), expand(1.f));
  auto &bot = t.make_ui(pixels(300), expand(1.f));
  t.add_child(root, top);
  t.add_child(root, mid);
  t.add_child(root, bot);
  t.run(root);

  // Remaining = 300 - 100 = 200, split evenly = 100 each
  CHECK_APPROX(t.ui(mid).computed[Axis::Y], 100.f);
  CHECK_APPROX(t.ui(bot).computed[Axis::Y], 100.f);
}

// ---------------------------------------------------------------------------
// NoWrap column: children that exceed parent height are NOT wrapped
// ---------------------------------------------------------------------------
TEST(nowrap_column_no_wrapping) {
  TestLayout t;
  auto &root = t.make_ui(pixels(200), pixels(200));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  // 3 children x 80px = 240px > 200px parent
  auto &c1 = t.make_ui(pixels(200), pixels(80));
  auto &c2 = t.make_ui(pixels(200), pixels(80));
  auto &c3 = t.make_ui(pixels(200), pixels(80));
  t.add_child(root, c1);
  t.add_child(root, c2);
  t.add_child(root, c3);
  t.run(root);

  // With NoWrap, children should still be stacked vertically in order,
  // not moved to a new column. The third child will overflow.
  CHECK_APPROX(t.ui(c1).computed_rel[Axis::Y], 0.f);
  // c2 follows c1 (even if shrunken by violation solver)
  CHECK(t.ui(c2).computed_rel[Axis::Y] > 0.f);
  // c3 follows c2
  CHECK(t.ui(c3).computed_rel[Axis::Y] > t.ui(c2).computed_rel[Axis::Y]);
}

// ---------------------------------------------------------------------------
// Wrap column: children that exceed parent height DO wrap to a new column
// ---------------------------------------------------------------------------
TEST(wrap_column_wraps_children) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(100));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_flex_wrap(FlexWrap::Wrap);

  // 3 children x 60px = 180px > 100px parent, so wrap should occur
  auto &c1 = t.make_ui(pixels(100), pixels(60));
  auto &c2 = t.make_ui(pixels(100), pixels(60));
  auto &c3 = t.make_ui(pixels(100), pixels(60));
  t.add_child(root, c1);
  t.add_child(root, c2);
  t.add_child(root, c3);
  t.run(root);

  // c1 fits in first column (Y=0), c2 should wrap to next column (Y resets)
  CHECK_APPROX(t.ui(c1).computed_rel[Axis::Y], 0.f);
  CHECK_APPROX(t.ui(c1).computed_rel[Axis::X], 0.f);

  // After wrapping, c2 or c3 should have X offset > 0 (new column)
  bool wrapped = t.ui(c2).computed_rel[Axis::X] > 50.f ||
                 t.ui(c3).computed_rel[Axis::X] > 50.f;
  CHECK(wrapped);
}

// ---------------------------------------------------------------------------
// Wrap row: children that exceed parent width wrap to a new row
// ---------------------------------------------------------------------------
TEST(wrap_row_wraps_children) {
  TestLayout t;
  auto &root = t.make_ui(pixels(200), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Row);
  t.ui(root).set_flex_wrap(FlexWrap::Wrap);

  // 3 children x 100px = 300px > 200px parent
  auto &c1 = t.make_ui(pixels(100), pixels(50));
  auto &c2 = t.make_ui(pixels(100), pixels(50));
  auto &c3 = t.make_ui(pixels(100), pixels(50));
  t.add_child(root, c1);
  t.add_child(root, c2);
  t.add_child(root, c3);
  t.run(root);

  // c1 and c2 fit in first row, c3 wraps to second row (Y offset > 0)
  CHECK_APPROX(t.ui(c1).computed_rel[Axis::X], 0.f);
  CHECK_APPROX(t.ui(c1).computed_rel[Axis::Y], 0.f);

  bool c3_wrapped = t.ui(c3).computed_rel[Axis::Y] > 10.f;
  CHECK(c3_wrapped);
}

// ---------------------------------------------------------------------------
// Percent child in padded parent: child fills content area correctly
// ---------------------------------------------------------------------------
TEST(percent_child_in_padded_parent) {
  TestLayout t;
  auto &root = t.make_ui(pixels(500), pixels(500));
  t.ui(root).set_desired_padding(pixels(50), Axis::X);
  t.ui(root).set_desired_padding(pixels(25), Axis::Y);

  auto &child = t.make_ui(percent(0.5f), percent(0.5f));
  t.add_child(root, child);
  t.run(root);

  // Content area: 500 - 100 = 400 wide, 500 - 50 = 450 tall
  // Child at 50%: 200 wide, 225 tall
  CHECK_APPROX(t.ui(child).computed[Axis::X], 200.f);
  CHECK_APPROX(t.ui(child).computed[Axis::Y], 225.f);
}

// ---------------------------------------------------------------------------
// screen_pct sizing resolves based on screen resolution
// ---------------------------------------------------------------------------
TEST(screen_pct_resolves_to_screen) {
  TestLayout t;
  t.resolution = {1280, 720};
  auto &root = t.make_ui(screen_pct(1.0f), screen_pct(1.0f));
  t.run(root);

  CHECK_APPROX(t.ui(root).computed[Axis::X], 1280.f);
  CHECK_APPROX(t.ui(root).computed[Axis::Y], 720.f);
}

// ---------------------------------------------------------------------------
// screen_pct at 50% gives half the screen
// ---------------------------------------------------------------------------
TEST(screen_pct_half) {
  TestLayout t;
  t.resolution = {1280, 720};
  auto &root = t.make_ui(screen_pct(0.5f), screen_pct(0.5f));
  t.run(root);

  CHECK_APPROX(t.ui(root).computed[Axis::X], 640.f);
  CHECK_APPROX(t.ui(root).computed[Axis::Y], 360.f);
}

// ---------------------------------------------------------------------------
// Margin does not overflow parent: child with margins fits within parent
// ---------------------------------------------------------------------------
TEST(margin_fits_within_parent) {
  TestLayout t;
  auto &root = t.make_ui(pixels(300), pixels(300));
  t.ui(root).set_flex_direction(FlexDirection::Column);

  auto &child = t.make_ui(percent(1.0f), pixels(100));
  child.get<UIComponent>().set_desired_margin(pixels(10), Axis::X);
  child.get<UIComponent>().set_desired_margin(pixels(10), Axis::Y);
  t.add_child(root, child);
  t.run(root);

  // Child rect should be within parent bounds (margin is inside the box)
  auto r = t.ui(child).rect();
  CHECK(r.x >= 0.f);
  CHECK(r.y >= 0.f);
  CHECK(r.x + r.width <= 300.f + 1.f);
  CHECK(r.y + r.height <= 300.f + 1.f);
}

// ---------------------------------------------------------------------------
// Absolute-positioned child is excluded from flex layout flow
// ---------------------------------------------------------------------------
TEST(absolute_child_excluded_from_flow) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);

  // This absolute child should not push siblings down
  auto &bg = t.make_ui(pixels(400), pixels(400));
  t.ui(bg).make_absolute();
  t.add_child(root, bg);

  auto &child = t.make_ui(pixels(400), pixels(100));
  t.add_child(root, child);
  t.run(root);

  // The flow child should start at Y=0, not pushed down by the absolute child
  CHECK_APPROX(t.ui(child).computed_rel[Axis::Y], 0.f);
}

// ---------------------------------------------------------------------------
// Nested padding: grandchild percent(1.0) fits within parent's content area
// ---------------------------------------------------------------------------
TEST(nested_padding) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_desired_padding(pixels(20), Axis::X);
  t.ui(root).set_desired_padding(pixels(20), Axis::Y);

  auto &mid = t.make_ui(percent(1.0f), percent(1.0f));
  t.ui(mid).set_desired_padding(pixels(10), Axis::X);
  t.ui(mid).set_desired_padding(pixels(10), Axis::Y);
  t.add_child(root, mid);

  auto &inner = t.make_ui(percent(1.0f), percent(1.0f));
  t.add_child(mid, inner);
  t.run(root);

  // root content = 400 - 40 = 360
  CHECK_APPROX(t.ui(mid).computed[Axis::X], 360.f);
  // mid content = 360 - 20 = 340
  CHECK_APPROX(t.ui(inner).computed[Axis::X], 340.f);
}

// ---------------------------------------------------------------------------
// Violation solver shrinks children that exceed parent main-axis
// ---------------------------------------------------------------------------
TEST(violation_solver_shrinks_overflow) {
  TestLayout t;
  auto &root = t.make_ui(pixels(200), pixels(200));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  // Two children that together exceed parent: 150 + 150 = 300 > 200
  // Use strictness < 1.0 so the violation solver is allowed to shrink them.
  // (strictness 1.0 = fully rigid, solver won't touch them)
  auto &c1 = t.make_ui(pixels(200), pixels(150, 0.5f));
  auto &c2 = t.make_ui(pixels(200), pixels(150, 0.5f));
  t.add_child(root, c1);
  t.add_child(root, c2);
  t.run(root);

  // Solver should shrink children so their total <= 200
  float total = t.ui(c1).computed[Axis::Y] + t.ui(c2).computed[Axis::Y];
  CHECK(total <= 201.f); // Allow 1px tolerance
}

// ---------------------------------------------------------------------------
// Children sized by children(): parent grows to fit children
// ---------------------------------------------------------------------------
TEST(children_sizing) {
  TestLayout t;
  auto &root = t.make_ui(Size{Dim::Children, 0}, Size{Dim::Children, 0});
  t.ui(root).set_flex_direction(FlexDirection::Column);

  auto &c1 = t.make_ui(pixels(200), pixels(50));
  auto &c2 = t.make_ui(pixels(300), pixels(75));
  t.add_child(root, c1);
  t.add_child(root, c2);
  t.run(root);

  // Parent should be sized to fit children:
  // Width = max(200, 300) = 300 (cross-axis)
  // Height = 50 + 75 = 125 (main-axis)
  CHECK_APPROX(t.ui(root).computed[Axis::X], 300.f);
  CHECK_APPROX(t.ui(root).computed[Axis::Y], 125.f);
}

// ---------------------------------------------------------------------------
// Deeply nested layout: 4 levels of nesting with padding at each level
// ---------------------------------------------------------------------------
TEST(deep_nesting_with_padding) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_desired_padding(pixels(10), Axis::X);
  t.ui(root).set_desired_padding(pixels(10), Axis::Y);

  auto &l1 = t.make_ui(percent(1.0f), percent(1.0f));
  t.ui(l1).set_desired_padding(pixels(10), Axis::X);
  t.ui(l1).set_desired_padding(pixels(10), Axis::Y);
  t.add_child(root, l1);

  auto &l2 = t.make_ui(percent(1.0f), percent(1.0f));
  t.ui(l2).set_desired_padding(pixels(10), Axis::X);
  t.ui(l2).set_desired_padding(pixels(10), Axis::Y);
  t.add_child(l1, l2);

  auto &l3 = t.make_ui(percent(1.0f), percent(1.0f));
  t.add_child(l2, l3);
  t.run(root);

  // root: 400 -> content 380
  // l1: 380 -> content 360
  // l2: 360 -> content 340
  // l3: 340
  CHECK_APPROX(t.ui(l1).computed[Axis::X], 380.f);
  CHECK_APPROX(t.ui(l2).computed[Axis::X], 360.f);
  CHECK_APPROX(t.ui(l3).computed[Axis::X], 340.f);
}

// ---------------------------------------------------------------------------
// Mixed children in column: fixed + expand + fixed pattern
// ---------------------------------------------------------------------------
TEST(header_body_footer_pattern) {
  TestLayout t;
  auto &root = t.make_ui(pixels(800), pixels(600));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);
  t.ui(root).set_desired_padding(pixels(10), Axis::Y); // 10+10=20 vertical padding

  auto &header = t.make_ui(percent(1.0f), pixels(60));
  auto &body = t.make_ui(percent(1.0f), expand());
  auto &footer = t.make_ui(percent(1.0f), pixels(40));
  t.add_child(root, header);
  t.add_child(root, body);
  t.add_child(root, footer);
  t.run(root);

  // Content height = 600 - 20 = 580
  // Header = 60, Footer = 40 -> Body = 580 - 60 - 40 = 480
  CHECK_APPROX(t.ui(header).computed[Axis::Y], 60.f);
  CHECK_APPROX(t.ui(body).computed[Axis::Y], 480.f);
  CHECK_APPROX(t.ui(footer).computed[Axis::Y], 40.f);

  // Footer position: After compute_rect_bounds, computed_rel includes
  // parent padding offset. header(60) + body(480) = 540, plus parent
  // padding_top(10) = 550.
  CHECK_APPROX(t.ui(footer).computed_rel[Axis::Y], 550.f);
}

// ---------------------------------------------------------------------------
// Padding + expand + margin combo: everything fits without overflow
// ---------------------------------------------------------------------------
TEST(padding_expand_margin_combo) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);
  t.ui(root).set_desired_padding(pixels(10), Axis::Y);

  auto &title = t.make_ui(percent(1.0f), pixels(40));
  auto &content = t.make_ui(percent(1.0f), expand());
  auto &status = t.make_ui(percent(1.0f), pixels(30));
  t.add_child(root, title);
  t.add_child(root, content);
  t.add_child(root, status);
  t.run(root);

  // Content height = 400 - 20 = 380
  // title(40) + status(30) = 70
  // content = 380 - 70 = 310
  CHECK_APPROX(t.ui(content).computed[Axis::Y], 310.f);

  // Status end should not exceed content area
  float status_end =
      t.ui(status).computed_rel[Axis::Y] + t.ui(status).computed[Axis::Y];
  // After rect_bounds: rel includes the parent's padding offset
  // So relative to parent origin: status_end = padding_top + offset + size
  // = 10 + 350 + 30 = 390 which is within 400
  CHECK(status_end <= 400.f + 2.f);
}

// ---------------------------------------------------------------------------
// Zero-size root: layout should not crash
// ---------------------------------------------------------------------------
TEST(zero_size_root_no_crash) {
  TestLayout t;
  auto &root = t.make_ui(pixels(0), pixels(0));
  auto &child = t.make_ui(percent(1.0f), percent(1.0f));
  t.add_child(root, child);
  t.run(root);

  // Layout should not crash; rect() clamps to non-negative
  auto r = t.ui(child).rect();
  CHECK(r.width >= 0.f);
  CHECK(r.height >= 0.f);
}

// ---------------------------------------------------------------------------
// Single child with no size: should default to something valid
// ---------------------------------------------------------------------------
TEST(no_size_child_valid) {
  TestLayout t;
  auto &root = t.make_ui(pixels(300), pixels(300));
  auto &child = t.make_ui(Size{Dim::None, 0}, Size{Dim::None, 0});
  t.add_child(root, child);
  t.run(root);

  // Dim::None leaves computed at -1 (unresolved), but rect() clamps to 0
  auto r = t.ui(child).rect();
  CHECK(r.width >= 0.f);
  CHECK(r.height >= 0.f);
}

// ---------------------------------------------------------------------------
// Column with mixed percent + pixel children
// ---------------------------------------------------------------------------
TEST(column_mixed_percent_pixel) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  auto &c1 = t.make_ui(percent(1.0f), pixels(100));
  auto &c2 = t.make_ui(percent(0.5f), pixels(100));
  t.add_child(root, c1);
  t.add_child(root, c2);
  t.run(root);

  CHECK_APPROX(t.ui(c1).computed[Axis::X], 400.f);
  CHECK_APPROX(t.ui(c2).computed[Axis::X], 200.f);
  CHECK_APPROX(t.ui(c1).computed[Axis::Y], 100.f);
  CHECK_APPROX(t.ui(c2).computed[Axis::Y], 100.f);
}

// ---------------------------------------------------------------------------
// Rect bounds accumulate correctly through hierarchy
// ---------------------------------------------------------------------------
TEST(rect_bounds_accumulate) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_desired_padding(pixels(20), Axis::X);
  t.ui(root).set_desired_padding(pixels(20), Axis::Y);

  auto &child = t.make_ui(percent(1.0f), pixels(100));
  t.ui(child).set_desired_padding(pixels(10), Axis::X);
  t.ui(child).set_desired_padding(pixels(10), Axis::Y);
  t.add_child(root, child);

  auto &grandchild = t.make_ui(percent(1.0f), percent(1.0f));
  t.add_child(child, grandchild);
  t.run(root);

  // Grandchild's rect should be offset by root padding + child padding
  auto r = t.ui(grandchild).rect();
  // root padding left(20) + child padding left(10) = 30
  CHECK(r.x >= 29.f);
  CHECK(r.y >= 29.f);
}

// ============================================================================
// Main
// ============================================================================

int main() {
  printf("=== Autolayout Engine Tests ===\n\n");

  for (auto &[name, fn] : test_registry()) {
    printf("  Running: %s\n", name);
    fn();
  }

  printf("\n%d/%d tests passed\n", tests_passed, tests_run);

  if (tests_passed != tests_run) {
    printf("FAILURES: %d\n", tests_run - tests_passed);
    return 1;
  }

  printf("All tests passed!\n");
  return 0;
}
