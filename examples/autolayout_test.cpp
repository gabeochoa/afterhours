// autolayout_test.cpp
// Comprehensive unit tests for the autolayout engine covering:
//
// Core sizing:
//   - Pixel, percent, screen_pct, children(), expand() sizing modes
//   - Padding correctly reduces content area (no double-counting)
//   - Negative dimensions are clamped to zero
//   - Cross-axis uses max (not sum) for children sizing
//   - Children() sizing includes padding in computed dimensions
//
// Flex layout:
//   - Column and Row stacking with correct offsets
//   - JustifyContent: FlexStart, FlexEnd, Center, SpaceBetween, SpaceAround
//   - AlignItems: FlexStart, FlexEnd, Center (Column and Row)
//   - SelfAlign: overrides parent's AlignItems per-child
//   - Combined Justify + Align centering
//
// Expand (flex-grow):
//   - Single expand fills remaining space
//   - Multiple expand shares space equally
//   - Weighted expand (expand(2) gets 2x space of expand(1))
//   - Expand with padding, nested expand
//   - Expand with zero remaining space
//
// Constraints:
//   - Min/max width and height (pixels, percent)
//   - Min prevents undersizing, max prevents oversizing
//   - Min/max bounded range
//
// Wrapping and overflow:
//   - NoWrap: items overflow but stay in order
//   - Wrap: column wraps to new columns, row wraps to new rows
//   - Wrap + children() sizing: parent grows to fit wrapped content
//   - Violation solver shrinks overflowing children
//
// Spacing:
//   - Symmetric and asymmetric padding
//   - Symmetric and asymmetric margins
//   - Margin stacking offsets in column and row
//   - Percent-based margins
//   - Padding offsets child positions correctly
//
// Positioning:
//   - Absolute children excluded from flow
//   - Multiple absolute children don't affect siblings
//   - rect() returns content-box, bounds() includes padding + margin
//   - Nested rect bounds accumulate through hierarchy
//
// Absolute positioning:
//   - Margins don't shrink absolute element size (position only)
//   - Margins position absolute elements correctly
//   - Large margins don't cause negative sizes on absolute elements
//   - Flow elements with large margins clamp to zero (contrast test)
//   - Absolute rect() vs bounds() includes margins/padding correctly
//   - Percent sizing resolves against parent for absolute elements
//   - Percent sizing + margins work together on absolute elements
//   - Padding inside absolute elements reduces child content area
//   - Absolute children excluded from parent children() sizing
//   - Multiple absolute children have independent margin positioning
//   - screen_pct sizing resolves against screen for absolute elements
//   - Absolute children interleaved with flow don't affect stacking
//
// Real-world patterns:
//   - Sidebar layout (fixed + expand)
//   - Dashboard (header + sidebar|main + footer)
//   - Holy grail (nav + main + aside with padding)
//   - Card grid (wrapping fixed-size items)
//   - Form layout (label + expand input rows)
//   - Deeply nested mixed Row/Column directions
//
// Edge cases:
//   - Zero-size root, no-size children
//   - Large padding leaving tiny content area
//   - Hidden children excluded from layout
//   - Deep nesting with padding at every level
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

  // UI scale for adaptive mode (passed to autolayout).
  float ui_scale = 1.0f;

  // Build entity map and run layout on the root entity.
  void run(Entity &root) {
    std::map<EntityID, RefEntity> mapping;
    for (auto &e : entities) {
      mapping.emplace(e->id, std::ref(*e));
    }
    AutoLayout::autolayout(root.get<UIComponent>(), resolution, mapping,
                           /*enable_grid_snapping=*/false, ui_scale);
  }

  // Create a UI entity with adaptive scaling mode.
  Entity &make_ui_adaptive(Size w, Size h) {
    Entity &e = make_ui(w, h);
    e.get<UIComponent>().resolved_scaling_mode = ScalingMode::Adaptive;
    return e;
  }

  // Set scaling mode on an existing entity.
  static void set_adaptive(Entity &e) {
    e.get<UIComponent>().resolved_scaling_mode = ScalingMode::Adaptive;
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
// JustifyContent tests
// ============================================================================

// ---------------------------------------------------------------------------
// JustifyContent::FlexStart (default): children packed at start
// ---------------------------------------------------------------------------
TEST(justify_flex_start_column) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_justify_content(JustifyContent::FlexStart);

  auto &c1 = t.make_ui(pixels(400), pixels(50));
  auto &c2 = t.make_ui(pixels(400), pixels(50));
  t.add_child(root, c1);
  t.add_child(root, c2);
  t.run(root);

  CHECK_APPROX(t.ui(c1).computed_rel[Axis::Y], 0.f);
  CHECK_APPROX(t.ui(c2).computed_rel[Axis::Y], 50.f);
}

// ---------------------------------------------------------------------------
// JustifyContent::FlexEnd: children packed at end
// ---------------------------------------------------------------------------
TEST(justify_flex_end_column) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_justify_content(JustifyContent::FlexEnd);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  auto &c1 = t.make_ui(pixels(400), pixels(50));
  auto &c2 = t.make_ui(pixels(400), pixels(50));
  t.add_child(root, c1);
  t.add_child(root, c2);
  t.run(root);

  // Remaining space = 400 - 100 = 300, start_offset = 300
  CHECK_APPROX(t.ui(c1).computed_rel[Axis::Y], 300.f);
  CHECK_APPROX(t.ui(c2).computed_rel[Axis::Y], 350.f);
}

// ---------------------------------------------------------------------------
// JustifyContent::Center: children centered
// ---------------------------------------------------------------------------
TEST(justify_center_column) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_justify_content(JustifyContent::Center);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  auto &c1 = t.make_ui(pixels(400), pixels(50));
  auto &c2 = t.make_ui(pixels(400), pixels(50));
  t.add_child(root, c1);
  t.add_child(root, c2);
  t.run(root);

  // Remaining space = 300, start_offset = 150
  CHECK_APPROX(t.ui(c1).computed_rel[Axis::Y], 150.f);
  CHECK_APPROX(t.ui(c2).computed_rel[Axis::Y], 200.f);
}

// ---------------------------------------------------------------------------
// JustifyContent::SpaceBetween: first at start, last at end, space between
// ---------------------------------------------------------------------------
TEST(justify_space_between_column) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_justify_content(JustifyContent::SpaceBetween);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  auto &c1 = t.make_ui(pixels(400), pixels(50));
  auto &c2 = t.make_ui(pixels(400), pixels(50));
  auto &c3 = t.make_ui(pixels(400), pixels(50));
  t.add_child(root, c1);
  t.add_child(root, c2);
  t.add_child(root, c3);
  t.run(root);

  // Remaining = 400 - 150 = 250, gap = 250 / 2 = 125
  CHECK_APPROX(t.ui(c1).computed_rel[Axis::Y], 0.f);
  CHECK_APPROX(t.ui(c2).computed_rel[Axis::Y], 175.f); // 50 + 125
  CHECK_APPROX(t.ui(c3).computed_rel[Axis::Y], 350.f); // 175 + 50 + 125
}

// ---------------------------------------------------------------------------
// JustifyContent::SpaceAround: equal space around each child
// ---------------------------------------------------------------------------
TEST(justify_space_around_column) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_justify_content(JustifyContent::SpaceAround);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  auto &c1 = t.make_ui(pixels(400), pixels(50));
  auto &c2 = t.make_ui(pixels(400), pixels(50));
  t.add_child(root, c1);
  t.add_child(root, c2);
  t.run(root);

  // Remaining = 400 - 100 = 300, gap = 300/2 = 150, start_offset = 75
  CHECK_APPROX(t.ui(c1).computed_rel[Axis::Y], 75.f);
  CHECK_APPROX(t.ui(c2).computed_rel[Axis::Y], 275.f); // 75 + 50 + 150
}

// ---------------------------------------------------------------------------
// JustifyContent::FlexEnd in Row direction
// ---------------------------------------------------------------------------
TEST(justify_flex_end_row) {
  TestLayout t;
  auto &root = t.make_ui(pixels(600), pixels(100));
  t.ui(root).set_flex_direction(FlexDirection::Row);
  t.ui(root).set_justify_content(JustifyContent::FlexEnd);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  auto &c1 = t.make_ui(pixels(100), pixels(100));
  auto &c2 = t.make_ui(pixels(100), pixels(100));
  t.add_child(root, c1);
  t.add_child(root, c2);
  t.run(root);

  // Remaining = 600 - 200 = 400, start_offset = 400
  CHECK_APPROX(t.ui(c1).computed_rel[Axis::X], 400.f);
  CHECK_APPROX(t.ui(c2).computed_rel[Axis::X], 500.f);
}

// ---------------------------------------------------------------------------
// JustifyContent::Center in Row direction
// ---------------------------------------------------------------------------
TEST(justify_center_row) {
  TestLayout t;
  auto &root = t.make_ui(pixels(600), pixels(100));
  t.ui(root).set_flex_direction(FlexDirection::Row);
  t.ui(root).set_justify_content(JustifyContent::Center);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  auto &c1 = t.make_ui(pixels(100), pixels(100));
  auto &c2 = t.make_ui(pixels(100), pixels(100));
  t.add_child(root, c1);
  t.add_child(root, c2);
  t.run(root);

  // Remaining = 400, start_offset = 200
  CHECK_APPROX(t.ui(c1).computed_rel[Axis::X], 200.f);
  CHECK_APPROX(t.ui(c2).computed_rel[Axis::X], 300.f);
}

// ---------------------------------------------------------------------------
// JustifyContent::SpaceBetween in Row with 2 items
// ---------------------------------------------------------------------------
TEST(justify_space_between_row) {
  TestLayout t;
  auto &root = t.make_ui(pixels(600), pixels(100));
  t.ui(root).set_flex_direction(FlexDirection::Row);
  t.ui(root).set_justify_content(JustifyContent::SpaceBetween);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  auto &c1 = t.make_ui(pixels(100), pixels(100));
  auto &c2 = t.make_ui(pixels(100), pixels(100));
  t.add_child(root, c1);
  t.add_child(root, c2);
  t.run(root);

  // 2 items: first at 0, second at end: gap = 400/1 = 400
  CHECK_APPROX(t.ui(c1).computed_rel[Axis::X], 0.f);
  CHECK_APPROX(t.ui(c2).computed_rel[Axis::X], 500.f); // 0 + 100 + 400
}

// ============================================================================
// AlignItems tests
// ============================================================================

// ---------------------------------------------------------------------------
// AlignItems::FlexStart (default): children at cross-axis start
// ---------------------------------------------------------------------------
TEST(align_items_flex_start_column) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_align_items(AlignItems::FlexStart);

  auto &child = t.make_ui(pixels(100), pixels(50));
  t.add_child(root, child);
  t.run(root);

  // Cross axis (X) should be at start
  CHECK_APPROX(t.ui(child).computed_rel[Axis::X], 0.f);
}

// ---------------------------------------------------------------------------
// AlignItems::FlexEnd: children at cross-axis end
// ---------------------------------------------------------------------------
TEST(align_items_flex_end_column) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_align_items(AlignItems::FlexEnd);

  auto &child = t.make_ui(pixels(100), pixels(50));
  t.add_child(root, child);
  t.run(root);

  // Cross axis remaining = 400 - 100 = 300, offset = 300
  CHECK_APPROX(t.ui(child).computed_rel[Axis::X], 300.f);
}

// ---------------------------------------------------------------------------
// AlignItems::Center: children centered on cross axis
// ---------------------------------------------------------------------------
TEST(align_items_center_column) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_align_items(AlignItems::Center);

  auto &child = t.make_ui(pixels(100), pixels(50));
  t.add_child(root, child);
  t.run(root);

  // Cross axis remaining = 300, center offset = 150
  CHECK_APPROX(t.ui(child).computed_rel[Axis::X], 150.f);
}

// ---------------------------------------------------------------------------
// AlignItems::FlexEnd in Row: children at bottom
// ---------------------------------------------------------------------------
TEST(align_items_flex_end_row) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Row);
  t.ui(root).set_align_items(AlignItems::FlexEnd);

  auto &child = t.make_ui(pixels(100), pixels(50));
  t.add_child(root, child);
  t.run(root);

  // In Row, cross axis is Y. Remaining = 400 - 50 = 350
  CHECK_APPROX(t.ui(child).computed_rel[Axis::Y], 350.f);
}

// ---------------------------------------------------------------------------
// AlignItems::Center in Row: children vertically centered
// ---------------------------------------------------------------------------
TEST(align_items_center_row) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Row);
  t.ui(root).set_align_items(AlignItems::Center);

  auto &child = t.make_ui(pixels(100), pixels(50));
  t.add_child(root, child);
  t.run(root);

  // Cross axis Y remaining = 350, center = 175
  CHECK_APPROX(t.ui(child).computed_rel[Axis::Y], 175.f);
}

// ---------------------------------------------------------------------------
// AlignItems with multiple children: each centered independently
// ---------------------------------------------------------------------------
TEST(align_items_center_multiple_children) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_align_items(AlignItems::Center);

  auto &c1 = t.make_ui(pixels(100), pixels(50));
  auto &c2 = t.make_ui(pixels(200), pixels(50));
  t.add_child(root, c1);
  t.add_child(root, c2);
  t.run(root);

  // c1: 400 - 100 = 300, center = 150
  CHECK_APPROX(t.ui(c1).computed_rel[Axis::X], 150.f);
  // c2: 400 - 200 = 200, center = 100
  CHECK_APPROX(t.ui(c2).computed_rel[Axis::X], 100.f);
}

// ============================================================================
// SelfAlign tests
// ============================================================================

// ---------------------------------------------------------------------------
// SelfAlign overrides parent AlignItems
// ---------------------------------------------------------------------------
TEST(self_align_overrides_parent) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_align_items(AlignItems::FlexStart);

  auto &c1 = t.make_ui(pixels(100), pixels(50));
  // c1 uses parent's FlexStart = X at 0

  auto &c2 = t.make_ui(pixels(100), pixels(50));
  t.ui(c2).set_self_align(SelfAlign::Center);
  // c2 overrides to Center

  auto &c3 = t.make_ui(pixels(100), pixels(50));
  t.ui(c3).set_self_align(SelfAlign::FlexEnd);
  // c3 overrides to FlexEnd

  t.add_child(root, c1);
  t.add_child(root, c2);
  t.add_child(root, c3);
  t.run(root);

  CHECK_APPROX(t.ui(c1).computed_rel[Axis::X], 0.f);    // FlexStart
  CHECK_APPROX(t.ui(c2).computed_rel[Axis::X], 150.f);   // Center: (400-100)/2
  CHECK_APPROX(t.ui(c3).computed_rel[Axis::X], 300.f);   // FlexEnd: 400-100
}

// ---------------------------------------------------------------------------
// SelfAlign::FlexStart overrides parent's Center
// ---------------------------------------------------------------------------
TEST(self_align_flex_start_overrides_center) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_align_items(AlignItems::Center);

  auto &c1 = t.make_ui(pixels(100), pixels(50));
  // c1 uses parent's Center

  auto &c2 = t.make_ui(pixels(100), pixels(50));
  t.ui(c2).set_self_align(SelfAlign::FlexStart);
  // c2 overrides to FlexStart

  t.add_child(root, c1);
  t.add_child(root, c2);
  t.run(root);

  CHECK_APPROX(t.ui(c1).computed_rel[Axis::X], 150.f); // Center
  CHECK_APPROX(t.ui(c2).computed_rel[Axis::X], 0.f);   // FlexStart override
}

// ---------------------------------------------------------------------------
// SelfAlign in Row layout: overrides vertical alignment
// ---------------------------------------------------------------------------
TEST(self_align_row) {
  TestLayout t;
  auto &root = t.make_ui(pixels(600), pixels(200));
  t.ui(root).set_flex_direction(FlexDirection::Row);
  t.ui(root).set_align_items(AlignItems::FlexStart);

  auto &c1 = t.make_ui(pixels(100), pixels(50));
  auto &c2 = t.make_ui(pixels(100), pixels(50));
  t.ui(c2).set_self_align(SelfAlign::FlexEnd);

  t.add_child(root, c1);
  t.add_child(root, c2);
  t.run(root);

  CHECK_APPROX(t.ui(c1).computed_rel[Axis::Y], 0.f);    // FlexStart
  CHECK_APPROX(t.ui(c2).computed_rel[Axis::Y], 150.f);   // FlexEnd: 200-50
}

// ============================================================================
// Min/Max size constraint tests
// ============================================================================

// ---------------------------------------------------------------------------
// Min width prevents shrinking below threshold
// ---------------------------------------------------------------------------
TEST(min_width_prevents_undersizing) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);

  auto &child = t.make_ui(percent(0.1f), pixels(100));
  t.ui(child).set_min_width(pixels(100));
  t.add_child(root, child);
  t.run(root);

  // 10% of 400 = 40, but min = 100
  CHECK(t.ui(child).computed[Axis::X] >= 99.f);
}

// ---------------------------------------------------------------------------
// Max width prevents growing beyond threshold
// ---------------------------------------------------------------------------
TEST(max_width_prevents_oversizing) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);

  auto &child = t.make_ui(percent(1.0f), pixels(100));
  t.ui(child).set_max_width(pixels(200));
  t.add_child(root, child);
  t.run(root);

  // 100% of 400 = 400, but max = 200
  CHECK(t.ui(child).computed[Axis::X] <= 201.f);
}

// ---------------------------------------------------------------------------
// Min height with expand
// ---------------------------------------------------------------------------
TEST(min_height_with_expand) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(100));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  // Header takes 80, body expands to 20, but min is 50
  auto &header = t.make_ui(pixels(400), pixels(80));
  auto &body = t.make_ui(pixels(400), expand());
  t.ui(body).set_min_height(pixels(50));
  t.add_child(root, header);
  t.add_child(root, body);
  t.run(root);

  // Body expand = 100 - 80 = 20, but min = 50
  CHECK(t.ui(body).computed[Axis::Y] >= 49.f);
}

// ---------------------------------------------------------------------------
// Max height clamps tall content
// ---------------------------------------------------------------------------
TEST(max_height_clamps) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));

  auto &child = t.make_ui(pixels(400), pixels(300));
  t.ui(child).set_max_height(pixels(150));
  t.add_child(root, child);
  t.run(root);

  CHECK(t.ui(child).computed[Axis::Y] <= 151.f);
}

// ---------------------------------------------------------------------------
// Min and Max together create a bounded range
// ---------------------------------------------------------------------------
TEST(min_max_bounded_range) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));

  auto &child = t.make_ui(percent(0.5f), pixels(100));
  t.ui(child).set_min_width(pixels(100));
  t.ui(child).set_max_width(pixels(250));
  t.add_child(root, child);
  t.run(root);

  // 50% of 400 = 200, which is within [100, 250]
  CHECK_APPROX(t.ui(child).computed[Axis::X], 200.f);
}

// ---------------------------------------------------------------------------
// Min larger than computed forces minimum
// ---------------------------------------------------------------------------
TEST(min_larger_than_computed) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));

  auto &child = t.make_ui(pixels(50), pixels(50));
  t.ui(child).set_min_width(pixels(200));
  t.ui(child).set_min_height(pixels(200));
  t.add_child(root, child);
  t.run(root);

  CHECK(t.ui(child).computed[Axis::X] >= 199.f);
  CHECK(t.ui(child).computed[Axis::Y] >= 199.f);
}

// ============================================================================
// Hidden children tests
// ============================================================================

// ---------------------------------------------------------------------------
// Hidden child is excluded from stacking offsets
// ---------------------------------------------------------------------------
TEST(hidden_child_excluded_from_stacking) {
  TestLayout t;
  auto &root = t.make_ui(pixels(300), pixels(300));
  t.ui(root).set_flex_direction(FlexDirection::Column);

  auto &c1 = t.make_ui(pixels(300), pixels(100));
  auto &c2 = t.make_ui(pixels(300), pixels(100));
  t.ui(c2).should_hide = true;
  auto &c3 = t.make_ui(pixels(300), pixels(100));
  t.add_child(root, c1);
  t.add_child(root, c2);
  t.add_child(root, c3);
  t.run(root);

  // c2 is hidden, so c3 should follow immediately after c1
  CHECK_APPROX(t.ui(c1).computed_rel[Axis::Y], 0.f);
  CHECK_APPROX(t.ui(c3).computed_rel[Axis::Y], 100.f);
}

// ---------------------------------------------------------------------------
// Hidden child excluded from violation solver
// ---------------------------------------------------------------------------
TEST(hidden_child_excluded_from_violations) {
  TestLayout t;
  auto &root = t.make_ui(pixels(200), pixels(200));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  auto &c1 = t.make_ui(pixels(200), pixels(100));
  auto &c2 = t.make_ui(pixels(200), pixels(200)); // Would overflow
  t.ui(c2).should_hide = true;
  auto &c3 = t.make_ui(pixels(200), pixels(100));
  t.add_child(root, c1);
  t.add_child(root, c2);
  t.add_child(root, c3);
  t.run(root);

  // Only c1 + c3 = 200, exactly fits without shrinking
  CHECK_APPROX(t.ui(c1).computed[Axis::Y], 100.f);
  CHECK_APPROX(t.ui(c3).computed[Axis::Y], 100.f);
}

// ============================================================================
// Asymmetric padding tests
// ============================================================================

// ---------------------------------------------------------------------------
// Asymmetric padding: different on each side
// ---------------------------------------------------------------------------
TEST(asymmetric_padding) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_desired_padding(Padding{
      .top = pixels(10),
      .left = pixels(20),
      .bottom = pixels(30),
      .right = pixels(40),
  });

  auto &child = t.make_ui(percent(1.0f), percent(1.0f));
  t.add_child(root, child);
  t.run(root);

  // Horizontal content = 400 - 20 - 40 = 340
  // Vertical content = 400 - 10 - 30 = 360
  CHECK_APPROX(t.ui(child).computed[Axis::X], 340.f);
  CHECK_APPROX(t.ui(child).computed[Axis::Y], 360.f);
}

// ---------------------------------------------------------------------------
// Asymmetric padding offsets child position correctly
// ---------------------------------------------------------------------------
TEST(asymmetric_padding_offsets_position) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_desired_padding(Padding{
      .top = pixels(15),
      .left = pixels(25),
      .bottom = pixels(5),
      .right = pixels(5),
  });

  auto &child = t.make_ui(pixels(100), pixels(100));
  t.add_child(root, child);
  t.run(root);

  // Child should be offset by padding_left=25, padding_top=15
  auto r = t.ui(child).rect();
  CHECK_APPROX(r.x, 25.f);
  CHECK_APPROX(r.y, 15.f);
}

// ---------------------------------------------------------------------------
// Asymmetric margin: different on each side
// ---------------------------------------------------------------------------
TEST(asymmetric_margin) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);

  auto &child = t.make_ui(pixels(300), pixels(100));
  t.ui(child).set_desired_margin(Margin{
      .top = pixels(10),
      .bottom = pixels(20),
      .left = pixels(30),
      .right = pixels(40),
  });
  t.add_child(root, child);
  t.run(root);

  // rect() includes margin offset
  auto r = t.ui(child).rect();
  CHECK_APPROX(r.x, 30.f); // margin left
  CHECK_APPROX(r.y, 10.f); // margin top
}

// ============================================================================
// Weighted expand tests
// ============================================================================

// ---------------------------------------------------------------------------
// expand(2) gets twice the space of expand(1)
// ---------------------------------------------------------------------------
TEST(weighted_expand_2_to_1) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(300));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  auto &c1 = t.make_ui(pixels(400), expand(1.f));
  auto &c2 = t.make_ui(pixels(400), expand(2.f));
  t.add_child(root, c1);
  t.add_child(root, c2);
  t.run(root);

  // Total weight = 3, space = 300
  // c1 = 300 * (1/3) = 100
  // c2 = 300 * (2/3) = 200
  CHECK_APPROX(t.ui(c1).computed[Axis::Y], 100.f);
  CHECK_APPROX(t.ui(c2).computed[Axis::Y], 200.f);
}

// ---------------------------------------------------------------------------
// expand(3) + expand(1) with fixed header
// ---------------------------------------------------------------------------
TEST(weighted_expand_with_fixed) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  auto &header = t.make_ui(pixels(400), pixels(100));
  auto &main = t.make_ui(pixels(400), expand(3.f));
  auto &sidebar = t.make_ui(pixels(400), expand(1.f));
  t.add_child(root, header);
  t.add_child(root, main);
  t.add_child(root, sidebar);
  t.run(root);

  // Remaining = 400 - 100 = 300
  // main = 300 * (3/4) = 225
  // sidebar = 300 * (1/4) = 75
  CHECK_APPROX(t.ui(header).computed[Axis::Y], 100.f);
  CHECK_APPROX(t.ui(main).computed[Axis::Y], 225.f);
  CHECK_APPROX(t.ui(sidebar).computed[Axis::Y], 75.f);
}

// ---------------------------------------------------------------------------
// Weighted expand in Row direction
// ---------------------------------------------------------------------------
TEST(weighted_expand_row) {
  TestLayout t;
  auto &root = t.make_ui(pixels(600), pixels(100));
  t.ui(root).set_flex_direction(FlexDirection::Row);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  auto &left = t.make_ui(pixels(100), pixels(100));
  auto &center = t.make_ui(expand(2.f), pixels(100));
  auto &right = t.make_ui(expand(1.f), pixels(100));
  t.add_child(root, left);
  t.add_child(root, center);
  t.add_child(root, right);
  t.run(root);

  // Remaining = 600 - 100 = 500
  // center = 500 * (2/3) ≈ 333.3
  // right = 500 * (1/3) ≈ 166.7
  float center_w = t.ui(center).computed[Axis::X];
  float right_w = t.ui(right).computed[Axis::X];
  CHECK(approx(center_w, right_w * 2.f, 2.f));
  CHECK(approx(center_w + right_w, 500.f, 2.f));
}

// ============================================================================
// Rect vs Bounds semantic tests
// ============================================================================

// ---------------------------------------------------------------------------
// rect() is content box (no margin, no padding)
// ---------------------------------------------------------------------------
TEST(rect_is_content_box) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));

  auto &child = t.make_ui(pixels(200), pixels(100));
  t.ui(child).set_desired_margin(pixels(10), Axis::X);
  t.ui(child).set_desired_margin(pixels(10), Axis::Y);
  t.ui(child).set_desired_padding(pixels(5), Axis::X);
  t.ui(child).set_desired_padding(pixels(5), Axis::Y);
  t.add_child(root, child);
  t.run(root);

  auto r = t.ui(child).rect();
  // rect width = computed - margin_x = 200 - 20 = 180
  // rect height = computed - margin_y = 100 - 20 = 80
  CHECK_APPROX(r.width, 180.f);
  CHECK_APPROX(r.height, 80.f);
  // rect position is offset by margin
  CHECK_APPROX(r.x, 10.f);
  CHECK_APPROX(r.y, 10.f);
}

// ---------------------------------------------------------------------------
// bounds() includes padding and margin
// ---------------------------------------------------------------------------
TEST(bounds_includes_padding_and_margin) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));

  auto &child = t.make_ui(pixels(200), pixels(100));
  t.ui(child).set_desired_margin(pixels(10), Axis::X);
  t.ui(child).set_desired_margin(pixels(10), Axis::Y);
  t.ui(child).set_desired_padding(pixels(5), Axis::X);
  t.ui(child).set_desired_padding(pixels(5), Axis::Y);
  t.add_child(root, child);
  t.run(root);

  auto b = t.ui(child).bounds();
  // bounds width = rect_width + padding_x + margin_x = 180 + 10 + 20 = 210
  // bounds height = rect_height + padding_y + margin_y = 80 + 10 + 20 = 110
  CHECK_APPROX(b.width, 210.f);
  CHECK_APPROX(b.height, 110.f);
  // bounds position is at element's origin (before margin)
  CHECK_APPROX(b.x, 0.f);
  CHECK_APPROX(b.y, 0.f);
}

// ---------------------------------------------------------------------------
// Root element rect() and bounds() at origin
// ---------------------------------------------------------------------------
TEST(root_rect_at_origin) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(300));
  t.run(root);

  auto r = t.ui(root).rect();
  CHECK_APPROX(r.x, 0.f);
  CHECK_APPROX(r.y, 0.f);
  CHECK_APPROX(r.width, 400.f);
  CHECK_APPROX(r.height, 300.f);
}

// ============================================================================
// Margin stacking offset tests
// ============================================================================

// ---------------------------------------------------------------------------
// Margins add to stacking offset in column layout
// ---------------------------------------------------------------------------
TEST(margin_stacking_column) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);

  auto &c1 = t.make_ui(pixels(400), pixels(80));
  t.ui(c1).set_desired_margin(Margin{.bottom = pixels(20)});

  auto &c2 = t.make_ui(pixels(400), pixels(80));
  t.ui(c2).set_desired_margin(Margin{.top = pixels(10)});

  t.add_child(root, c1);
  t.add_child(root, c2);
  t.run(root);

  // c1 occupies: size(80) + margin_y(20 top+bottom, actually just bottom=20)
  // c2 should be offset by c1's computed + c1's margin
  float c2_start = t.ui(c2).computed_rel[Axis::Y];
  // c1 computed[Y] includes margin space, so c2 follows after
  CHECK(c2_start >= 80.f); // At minimum after c1's content
}

// ---------------------------------------------------------------------------
// Margins in row layout create horizontal spacing
// ---------------------------------------------------------------------------
TEST(margin_stacking_row) {
  TestLayout t;
  auto &root = t.make_ui(pixels(600), pixels(100));
  t.ui(root).set_flex_direction(FlexDirection::Row);

  auto &c1 = t.make_ui(pixels(100), pixels(100));
  t.ui(c1).set_desired_margin(Margin{.right = pixels(20)});

  auto &c2 = t.make_ui(pixels(100), pixels(100));
  t.ui(c2).set_desired_margin(Margin{.left = pixels(10)});

  t.add_child(root, c1);
  t.add_child(root, c2);
  t.run(root);

  // c2 should be offset by c1's computed + margins
  float c2_start = t.ui(c2).computed_rel[Axis::X];
  CHECK(c2_start >= 100.f);
}

// ============================================================================
// Wrap + children() resize tests
// ============================================================================

// ---------------------------------------------------------------------------
// Row wrap with children() height: parent grows to fit wrapped rows
// ---------------------------------------------------------------------------
TEST(wrap_row_children_height_grows) {
  TestLayout t;
  auto &root = t.make_ui(pixels(200), Size{Dim::Children, 0});
  t.ui(root).set_flex_direction(FlexDirection::Row);
  t.ui(root).set_flex_wrap(FlexWrap::Wrap);

  // 3 x 100px wide in 200px container = wraps to 2 rows
  auto &c1 = t.make_ui(pixels(100), pixels(50));
  auto &c2 = t.make_ui(pixels(100), pixels(50));
  auto &c3 = t.make_ui(pixels(100), pixels(50));
  t.add_child(root, c1);
  t.add_child(root, c2);
  t.add_child(root, c3);
  t.run(root);

  // Should have 2 rows of 50px each = 100px total height
  CHECK(t.ui(root).computed[Axis::Y] >= 99.f);
}

// ---------------------------------------------------------------------------
// Column wrap with children() width: parent grows to fit wrapped columns
// ---------------------------------------------------------------------------
TEST(wrap_column_children_width_grows) {
  TestLayout t;
  auto &root = t.make_ui(Size{Dim::Children, 0}, pixels(100));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_flex_wrap(FlexWrap::Wrap);

  // 3 x 60px tall in 100px container = wraps
  auto &c1 = t.make_ui(pixels(80), pixels(60));
  auto &c2 = t.make_ui(pixels(80), pixels(60));
  auto &c3 = t.make_ui(pixels(80), pixels(60));
  t.add_child(root, c1);
  t.add_child(root, c2);
  t.add_child(root, c3);
  t.run(root);

  // Should have at least 2 columns = 160px+ width
  CHECK(t.ui(root).computed[Axis::X] >= 159.f);
}

// ============================================================================
// Real-world layout pattern tests
// ============================================================================

// ---------------------------------------------------------------------------
// Sidebar layout: fixed sidebar + expand content
// ---------------------------------------------------------------------------
TEST(sidebar_layout) {
  TestLayout t;
  auto &root = t.make_ui(pixels(800), pixels(600));
  t.ui(root).set_flex_direction(FlexDirection::Row);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  auto &sidebar = t.make_ui(pixels(200), percent(1.0f));
  auto &content = t.make_ui(expand(), percent(1.0f));
  t.add_child(root, sidebar);
  t.add_child(root, content);
  t.run(root);

  CHECK_APPROX(t.ui(sidebar).computed[Axis::X], 200.f);
  CHECK_APPROX(t.ui(content).computed[Axis::X], 600.f); // 800 - 200
  CHECK_APPROX(t.ui(sidebar).computed[Axis::Y], 600.f);
  CHECK_APPROX(t.ui(content).computed[Axis::Y], 600.f);
}

// ---------------------------------------------------------------------------
// Dashboard: header + (sidebar | main) + footer
// ---------------------------------------------------------------------------
TEST(dashboard_layout) {
  TestLayout t;
  auto &root = t.make_ui(pixels(800), pixels(600));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  auto &header = t.make_ui(percent(1.0f), pixels(60));
  auto &body = t.make_ui(percent(1.0f), expand());
  t.ui(body).set_flex_direction(FlexDirection::Row);
  t.ui(body).set_flex_wrap(FlexWrap::NoWrap);
  auto &footer = t.make_ui(percent(1.0f), pixels(40));

  t.add_child(root, header);
  t.add_child(root, body);
  t.add_child(root, footer);

  // Body children
  auto &sidebar = t.make_ui(pixels(200), percent(1.0f));
  auto &main = t.make_ui(expand(), percent(1.0f));
  t.add_child(body, sidebar);
  t.add_child(body, main);

  t.run(root);

  // Header
  CHECK_APPROX(t.ui(header).computed[Axis::Y], 60.f);
  CHECK_APPROX(t.ui(header).computed[Axis::X], 800.f);

  // Body
  float body_h = t.ui(body).computed[Axis::Y];
  CHECK_APPROX(body_h, 500.f); // 600 - 60 - 40

  // Footer
  CHECK_APPROX(t.ui(footer).computed[Axis::Y], 40.f);

  // Sidebar and main within body
  CHECK_APPROX(t.ui(sidebar).computed[Axis::X], 200.f);
  CHECK_APPROX(t.ui(main).computed[Axis::X], 600.f); // 800 - 200
}

// ---------------------------------------------------------------------------
// Card grid: wrapping row of fixed-size cards
// ---------------------------------------------------------------------------
TEST(card_grid_layout) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(600));
  t.ui(root).set_flex_direction(FlexDirection::Row);
  t.ui(root).set_flex_wrap(FlexWrap::Wrap);

  // 5 cards x 120px wide in 400px container = 3 per row, 2 rows
  for (int i = 0; i < 5; i++) {
    auto &card = t.make_ui(pixels(120), pixels(80));
    t.add_child(root, card);
  }
  t.run(root);

  // First card at (0,0)
  auto &c0 = t.ui(*t.entities[1]);
  CHECK_APPROX(c0.computed_rel[Axis::X], 0.f);
  CHECK_APPROX(c0.computed_rel[Axis::Y], 0.f);

  // Fourth card should be on second row (Y > 0)
  auto &c3 = t.ui(*t.entities[4]);
  CHECK(c3.computed_rel[Axis::Y] > 50.f);
}

// ---------------------------------------------------------------------------
// Holy grail: header + (nav | main | aside) + footer with padding
// ---------------------------------------------------------------------------
TEST(holy_grail_layout) {
  TestLayout t;
  auto &root = t.make_ui(pixels(1000), pixels(700));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);
  t.ui(root).set_desired_padding(pixels(10), Axis::X);
  t.ui(root).set_desired_padding(pixels(10), Axis::Y);

  auto &header = t.make_ui(percent(1.0f), pixels(80));
  auto &body = t.make_ui(percent(1.0f), expand());
  t.ui(body).set_flex_direction(FlexDirection::Row);
  t.ui(body).set_flex_wrap(FlexWrap::NoWrap);
  auto &footer = t.make_ui(percent(1.0f), pixels(60));

  t.add_child(root, header);
  t.add_child(root, body);
  t.add_child(root, footer);

  auto &nav = t.make_ui(pixels(150), percent(1.0f));
  auto &main = t.make_ui(expand(), percent(1.0f));
  auto &aside = t.make_ui(pixels(200), percent(1.0f));
  t.add_child(body, nav);
  t.add_child(body, main);
  t.add_child(body, aside);

  t.run(root);

  // Content area = 1000-20 x 700-20 = 980 x 680
  CHECK_APPROX(t.ui(header).computed[Axis::X], 980.f);
  CHECK_APPROX(t.ui(header).computed[Axis::Y], 80.f);
  CHECK_APPROX(t.ui(footer).computed[Axis::Y], 60.f);

  // Body = 680 - 80 - 60 = 540
  CHECK_APPROX(t.ui(body).computed[Axis::Y], 540.f);

  // Nav + Main + Aside within body (980 wide)
  CHECK_APPROX(t.ui(nav).computed[Axis::X], 150.f);
  CHECK_APPROX(t.ui(aside).computed[Axis::X], 200.f);
  CHECK_APPROX(t.ui(main).computed[Axis::X], 630.f); // 980 - 150 - 200
}

// ---------------------------------------------------------------------------
// Form layout: labels left-aligned, inputs expand to fill
// ---------------------------------------------------------------------------
TEST(form_row_layout) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(300));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  // Create 3 form rows
  for (int i = 0; i < 3; i++) {
    auto &row = t.make_ui(percent(1.0f), pixels(40));
    t.ui(row).set_flex_direction(FlexDirection::Row);
    t.ui(row).set_flex_wrap(FlexWrap::NoWrap);

    auto &label = t.make_ui(pixels(100), percent(1.0f));
    auto &input = t.make_ui(expand(), percent(1.0f));

    t.add_child(row, label);
    t.add_child(row, input);
    t.add_child(root, row);
  }
  t.run(root);

  // Each row should be 400 wide, 40 tall
  // Label = 100, Input = 300
  for (int i = 0; i < 3; i++) {
    // Row entities start at index 1 (after root), each row is entities[1+i*3]
    auto &row = t.ui(*t.entities[1 + i * 3]);
    CHECK_APPROX(row.computed[Axis::X], 400.f);
    CHECK_APPROX(row.computed[Axis::Y], 40.f);
  }
}

// ---------------------------------------------------------------------------
// Nested expand: parent expand + child expand
// ---------------------------------------------------------------------------
TEST(nested_expand) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  auto &header = t.make_ui(percent(1.0f), pixels(50));
  auto &body = t.make_ui(percent(1.0f), expand());
  t.ui(body).set_flex_direction(FlexDirection::Column);
  t.ui(body).set_flex_wrap(FlexWrap::NoWrap);

  t.add_child(root, header);
  t.add_child(root, body);

  // Body has its own header and expand child
  auto &body_header = t.make_ui(percent(1.0f), pixels(30));
  auto &body_content = t.make_ui(percent(1.0f), expand());
  t.add_child(body, body_header);
  t.add_child(body, body_content);

  t.run(root);

  // Body = 400 - 50 = 350
  CHECK_APPROX(t.ui(body).computed[Axis::Y], 350.f);
  // Body content = 350 - 30 = 320
  CHECK_APPROX(t.ui(body_content).computed[Axis::Y], 320.f);
}

// ---------------------------------------------------------------------------
// Multiple absolute children don't affect flow
// ---------------------------------------------------------------------------
TEST(multiple_absolute_children) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);

  auto &bg1 = t.make_ui(pixels(400), pixels(400));
  t.ui(bg1).make_absolute();
  auto &bg2 = t.make_ui(pixels(200), pixels(200));
  t.ui(bg2).make_absolute();

  auto &c1 = t.make_ui(pixels(400), pixels(100));
  auto &c2 = t.make_ui(pixels(400), pixels(100));

  t.add_child(root, bg1);
  t.add_child(root, bg2);
  t.add_child(root, c1);
  t.add_child(root, c2);
  t.run(root);

  // Flow children should start at Y=0, unaffected by absolutes
  CHECK_APPROX(t.ui(c1).computed_rel[Axis::Y], 0.f);
  CHECK_APPROX(t.ui(c2).computed_rel[Axis::Y], 100.f);
}

// ---------------------------------------------------------------------------
// Justify + Align combined: center both axes
// ---------------------------------------------------------------------------
TEST(justify_center_align_center) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_justify_content(JustifyContent::Center);
  t.ui(root).set_align_items(AlignItems::Center);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  auto &child = t.make_ui(pixels(100), pixels(100));
  t.add_child(root, child);
  t.run(root);

  // Main axis (Y): (400-100)/2 = 150
  CHECK_APPROX(t.ui(child).computed_rel[Axis::Y], 150.f);
  // Cross axis (X): (400-100)/2 = 150
  CHECK_APPROX(t.ui(child).computed_rel[Axis::X], 150.f);
}

// ---------------------------------------------------------------------------
// JustifyContent with padding: space calculation respects padding
// ---------------------------------------------------------------------------
TEST(justify_center_with_padding) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_justify_content(JustifyContent::Center);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);
  t.ui(root).set_desired_padding(pixels(20), Axis::Y);

  auto &child = t.make_ui(pixels(400), pixels(100));
  t.add_child(root, child);
  t.run(root);

  // Content area = 400 - 40 = 360, remaining = 260, center offset = 130
  // Child relative position should be centered within content area
  // After compute_rect_bounds, the child's rel includes the parent's
  // padding offset (20), so the absolute position = 20 + 130 = 150
  // But computed_rel before rect_bounds should be 130
  // Actually after the full layout, computed_rel includes parent offset
  CHECK_APPROX(t.ui(child).computed_rel[Axis::Y], 150.f); // 20 padding + 130 center
}

// ---------------------------------------------------------------------------
// AlignItems with padding: cross-axis uses content area
// ---------------------------------------------------------------------------
TEST(align_center_with_padding) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_align_items(AlignItems::Center);
  t.ui(root).set_desired_padding(pixels(20), Axis::X);

  auto &child = t.make_ui(pixels(100), pixels(50));
  t.add_child(root, child);
  t.run(root);

  // Content width = 400 - 40 = 360
  // Cross offset = (360 - 100) / 2 = 130
  // Plus padding_left(20) from rect_bounds = 150
  CHECK_APPROX(t.ui(child).computed_rel[Axis::X], 150.f);
}

// ---------------------------------------------------------------------------
// Expand fills zero when all space taken by fixed children
// ---------------------------------------------------------------------------
TEST(expand_zero_remaining) {
  TestLayout t;
  auto &root = t.make_ui(pixels(200), pixels(200));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  auto &c1 = t.make_ui(pixels(200), pixels(100));
  auto &c2 = t.make_ui(pixels(200), pixels(100));
  auto &expander = t.make_ui(pixels(200), expand());
  t.add_child(root, c1);
  t.add_child(root, c2);
  t.add_child(root, expander);
  t.run(root);

  // c1 + c2 = 200, no remaining space
  // expand should get 0 (or near 0)
  CHECK(t.ui(expander).computed[Axis::Y] <= 1.f);
}

// ---------------------------------------------------------------------------
// Percent of padded parent: 50% of content area
// ---------------------------------------------------------------------------
TEST(percent_50_of_padded_parent) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_desired_padding(pixels(40), Axis::X);
  t.ui(root).set_desired_padding(pixels(40), Axis::Y);

  auto &child = t.make_ui(percent(0.5f), percent(0.5f));
  t.add_child(root, child);
  t.run(root);

  // Content = 400 - 80 = 320
  // 50% = 160
  CHECK_APPROX(t.ui(child).computed[Axis::X], 160.f);
  CHECK_APPROX(t.ui(child).computed[Axis::Y], 160.f);
}

// ---------------------------------------------------------------------------
// Children() sizing with padding includes padding in computed
// ---------------------------------------------------------------------------
TEST(children_sizing_includes_padding) {
  TestLayout t;
  auto &root = t.make_ui(Size{Dim::Children, 0}, Size{Dim::Children, 0});
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_desired_padding(pixels(15), Axis::X);
  t.ui(root).set_desired_padding(pixels(15), Axis::Y);

  auto &child = t.make_ui(pixels(100), pixels(50));
  t.add_child(root, child);
  t.run(root);

  // Root computed should be child + padding
  // Width = 100 + 30 = 130, Height = 50 + 30 = 80
  CHECK_APPROX(t.ui(root).computed[Axis::X], 130.f);
  CHECK_APPROX(t.ui(root).computed[Axis::Y], 80.f);
}

// ---------------------------------------------------------------------------
// SpaceBetween with single child: no gap, child at start
// ---------------------------------------------------------------------------
TEST(justify_space_between_single_child) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_justify_content(JustifyContent::SpaceBetween);

  auto &child = t.make_ui(pixels(400), pixels(100));
  t.add_child(root, child);
  t.run(root);

  // Single child with SpaceBetween: gap = 0, start at 0
  CHECK_APPROX(t.ui(child).computed_rel[Axis::Y], 0.f);
}

// ---------------------------------------------------------------------------
// Large padding that nearly fills parent: child gets tiny content area
// ---------------------------------------------------------------------------
TEST(large_padding_tiny_content) {
  TestLayout t;
  auto &root = t.make_ui(pixels(100), pixels(100));
  t.ui(root).set_desired_padding(pixels(45), Axis::X);
  t.ui(root).set_desired_padding(pixels(45), Axis::Y);

  auto &child = t.make_ui(percent(1.0f), percent(1.0f));
  t.add_child(root, child);
  t.run(root);

  // Content = 100 - 90 = 10
  CHECK_APPROX(t.ui(child).computed[Axis::X], 10.f);
  CHECK_APPROX(t.ui(child).computed[Axis::Y], 10.f);
}

// ---------------------------------------------------------------------------
// NoWrap overflow: items overflow but stay in order
// ---------------------------------------------------------------------------
TEST(nowrap_overflow_maintains_order) {
  TestLayout t;
  auto &root = t.make_ui(pixels(300), pixels(100));
  t.ui(root).set_flex_direction(FlexDirection::Row);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  auto &c1 = t.make_ui(pixels(150), pixels(100));
  auto &c2 = t.make_ui(pixels(150), pixels(100));
  auto &c3 = t.make_ui(pixels(150), pixels(100));
  t.add_child(root, c1);
  t.add_child(root, c2);
  t.add_child(root, c3);
  t.run(root);

  // All items should remain in order on same row (Y = 0)
  CHECK_APPROX(t.ui(c1).computed_rel[Axis::Y], 0.f);
  CHECK_APPROX(t.ui(c2).computed_rel[Axis::Y], 0.f);
  CHECK_APPROX(t.ui(c3).computed_rel[Axis::Y], 0.f);

  // X positions should be in order
  CHECK(t.ui(c1).computed_rel[Axis::X] < t.ui(c2).computed_rel[Axis::X]);
  CHECK(t.ui(c2).computed_rel[Axis::X] < t.ui(c3).computed_rel[Axis::X]);
}

// ---------------------------------------------------------------------------
// Percent margin resolves against parent
// ---------------------------------------------------------------------------
TEST(percent_margin_resolves) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);

  auto &child = t.make_ui(pixels(200), pixels(100));
  // 10% margin on all sides
  t.ui(child).set_desired_margin(Margin{
      .top = percent(0.1f),
      .bottom = percent(0.1f),
      .left = percent(0.1f),
      .right = percent(0.1f),
  });
  t.add_child(root, child);
  t.run(root);

  // Margin should be resolved: 10% of parent = 40 on X sides, 40 on Y sides
  // rect() position includes margin offset
  auto r = t.ui(child).rect();
  CHECK(r.x >= 30.f); // At least some margin left
  CHECK(r.y >= 30.f); // At least some margin top
}

// ---------------------------------------------------------------------------
// Complex nested: row inside column inside row
// ---------------------------------------------------------------------------
TEST(deeply_nested_mixed_directions) {
  TestLayout t;
  auto &root = t.make_ui(pixels(600), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Row);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  auto &left = t.make_ui(pixels(200), percent(1.0f));
  t.ui(left).set_flex_direction(FlexDirection::Column);
  t.ui(left).set_flex_wrap(FlexWrap::NoWrap);

  auto &right = t.make_ui(expand(), percent(1.0f));
  t.ui(right).set_flex_direction(FlexDirection::Column);
  t.ui(right).set_flex_wrap(FlexWrap::NoWrap);

  t.add_child(root, left);
  t.add_child(root, right);

  // Left column children
  auto &l1 = t.make_ui(percent(1.0f), pixels(100));
  auto &l2 = t.make_ui(percent(1.0f), expand());
  t.add_child(left, l1);
  t.add_child(left, l2);

  // Right column with nested row
  auto &r_header = t.make_ui(percent(1.0f), pixels(50));
  auto &r_body = t.make_ui(percent(1.0f), expand());
  t.ui(r_body).set_flex_direction(FlexDirection::Row);
  t.ui(r_body).set_flex_wrap(FlexWrap::NoWrap);
  t.add_child(right, r_header);
  t.add_child(right, r_body);

  // NOTE: Using pixels inside expand-parent row because percent(1.0)
  // children of expand()-sized parents don't re-resolve after expand
  // distributes space (known engine limitation - percent resolves when
  // parent computed is still 0).
  auto &rb1 = t.make_ui(pixels(200), percent(1.0f));
  auto &rb2 = t.make_ui(pixels(100), percent(1.0f));
  t.add_child(r_body, rb1);
  t.add_child(r_body, rb2);

  t.run(root);

  // Left
  CHECK_APPROX(t.ui(left).computed[Axis::X], 200.f);
  CHECK_APPROX(t.ui(left).computed[Axis::Y], 400.f);
  CHECK_APPROX(t.ui(l1).computed[Axis::Y], 100.f);
  CHECK_APPROX(t.ui(l2).computed[Axis::Y], 300.f);

  // Right
  float right_w = t.ui(right).computed[Axis::X];
  CHECK_APPROX(right_w, 400.f); // 600 - 200
  CHECK_APPROX(t.ui(r_header).computed[Axis::Y], 50.f);

  float r_body_h = t.ui(r_body).computed[Axis::Y];
  CHECK_APPROX(r_body_h, 350.f); // 400 - 50

  // Row within right body
  CHECK_APPROX(t.ui(rb2).computed[Axis::X], 100.f);
  CHECK_APPROX(t.ui(rb1).computed[Axis::X], 200.f);
}

// ============================================================================
// Absolute positioning
// ============================================================================

// ---------------------------------------------------------------------------
// Absolute + margin: margins don't shrink the element size
// ---------------------------------------------------------------------------
TEST(absolute_margin_no_shrink) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);

  auto &child = t.make_ui(pixels(200), pixels(100));
  t.ui(child).make_absolute();
  t.ui(child).set_desired_margin(pixels(50), Axis::X);
  t.ui(child).set_desired_margin(pixels(30), Axis::Y);
  t.add_child(root, child);
  t.run(root);

  // For absolute elements, rect() should NOT subtract margins from size
  auto r = t.ui(child).rect();
  CHECK_APPROX(r.width, 200.f);
  CHECK_APPROX(r.height, 100.f);
}

// ---------------------------------------------------------------------------
// Absolute + margin: margins position the element
// ---------------------------------------------------------------------------
TEST(absolute_margin_positions) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);

  auto &child = t.make_ui(pixels(100), pixels(100));
  t.ui(child).make_absolute();
  t.ui(child).set_desired_margin(Margin{
      .top = pixels(20),
      .bottom = pixels(0),
      .left = pixels(30),
      .right = pixels(0),
  });
  t.add_child(root, child);
  t.run(root);

  // Margins should offset the position of the absolute element
  auto r = t.ui(child).rect();
  CHECK_APPROX(r.x, 30.f);
  CHECK_APPROX(r.y, 20.f);
  // Size should be unchanged
  CHECK_APPROX(r.width, 100.f);
  CHECK_APPROX(r.height, 100.f);
}

// ---------------------------------------------------------------------------
// Absolute + large margin: even very large margins don't cause negative size
// ---------------------------------------------------------------------------
TEST(absolute_large_margin_no_negative) {
  TestLayout t;
  auto &root = t.make_ui(pixels(1280), pixels(720));

  auto &child = t.make_ui(screen_pct(0.4f), screen_pct(0.6f));
  t.ui(child).make_absolute();
  // Margins larger than the element — would cause negative size in flow
  t.ui(child).set_desired_margin(Margin{
      .top = screen_pct(0.3f),
      .bottom = screen_pct(0.3f),
      .left = screen_pct(0.3f),
      .right = screen_pct(0.3f),
  });
  t.add_child(root, child);
  t.run(root);

  auto r = t.ui(child).rect();
  // Size should be the screen_pct value, not reduced by margins
  // 0.4 * 1280 = 512, 0.6 * 720 = 432
  CHECK_APPROX(r.width, 512.f);
  CHECK_APPROX(r.height, 432.f);
  CHECK(r.width > 0.f);
  CHECK(r.height > 0.f);
}

// ---------------------------------------------------------------------------
// Flow + large margin: flow element clamps to zero (contrast with absolute)
// ---------------------------------------------------------------------------
TEST(flow_large_margin_clamps_to_zero) {
  TestLayout t;
  auto &root = t.make_ui(pixels(1280), pixels(720));
  t.ui(root).set_flex_direction(FlexDirection::Column);

  auto &child = t.make_ui(screen_pct(0.4f), screen_pct(0.6f));
  // Same margins that would cause negative size in flow
  t.ui(child).set_desired_margin(Margin{
      .top = screen_pct(0.3f),
      .bottom = screen_pct(0.3f),
      .left = screen_pct(0.3f),
      .right = screen_pct(0.3f),
  });
  t.add_child(root, child);
  t.run(root);

  auto r = t.ui(child).rect();
  // Flow layout subtracts margins: 512 - 384 - 384 < 0, clamped to 0
  CHECK_APPROX(r.width, 0.f);
  CHECK_APPROX(r.height, 0.f);
}

// ---------------------------------------------------------------------------
// Absolute rect() vs bounds(): rect is content-box, bounds includes margins
// ---------------------------------------------------------------------------
TEST(absolute_rect_vs_bounds) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));

  auto &child = t.make_ui(pixels(200), pixels(100));
  t.ui(child).make_absolute();
  t.ui(child).set_desired_margin(pixels(10), Axis::X);
  t.ui(child).set_desired_margin(pixels(10), Axis::Y);
  t.ui(child).set_desired_padding(pixels(5), Axis::X);
  t.ui(child).set_desired_padding(pixels(5), Axis::Y);
  t.add_child(root, child);
  t.run(root);

  auto r = t.ui(child).rect();
  // Absolute: rect width = computed (no margin subtraction)
  CHECK_APPROX(r.width, 200.f);
  CHECK_APPROX(r.height, 100.f);
  // Position offset by margin
  CHECK_APPROX(r.x, 10.f);
  CHECK_APPROX(r.y, 10.f);

  auto b = t.ui(child).bounds();
  // bounds includes padding + margin around the rect
  CHECK_APPROX(b.x, 0.f); // rect.x - margin_left = 10 - 10 = 0
  CHECK_APPROX(b.y, 0.f);
  CHECK_APPROX(b.width, 200.f + 10.f + 20.f);  // rect_w + pad_x + margin_x
  CHECK_APPROX(b.height, 100.f + 10.f + 20.f);  // rect_h + pad_y + margin_y
}

// ---------------------------------------------------------------------------
// Absolute + percent sizing resolves against parent
// ---------------------------------------------------------------------------
TEST(absolute_percent_resolves_against_parent) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(300));

  auto &child = t.make_ui(percent(0.5f), percent(0.5f));
  t.ui(child).make_absolute();
  t.add_child(root, child);
  t.run(root);

  auto r = t.ui(child).rect();
  CHECK_APPROX(r.width, 200.f);  // 50% of 400
  CHECK_APPROX(r.height, 150.f); // 50% of 300
}

// ---------------------------------------------------------------------------
// Absolute + percent + margin: percent resolves correctly, margin positions
// ---------------------------------------------------------------------------
TEST(absolute_percent_with_margin) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));

  auto &child = t.make_ui(percent(0.5f), percent(0.5f));
  t.ui(child).make_absolute();
  t.ui(child).set_desired_margin(Margin{
      .top = pixels(20),
      .bottom = pixels(0),
      .left = pixels(40),
      .right = pixels(0),
  });
  t.add_child(root, child);
  t.run(root);

  auto r = t.ui(child).rect();
  // Size should be 50% of parent, not reduced by margins
  CHECK_APPROX(r.width, 200.f);
  CHECK_APPROX(r.height, 200.f);
  // Position offset by margins
  CHECK_APPROX(r.x, 40.f);
  CHECK_APPROX(r.y, 20.f);
}

// ---------------------------------------------------------------------------
// Absolute + padding: padding reduces content area but not element size
// ---------------------------------------------------------------------------
TEST(absolute_with_padding) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));

  auto &parent = t.make_ui(pixels(200), pixels(200));
  t.ui(parent).make_absolute();
  t.ui(parent).set_desired_padding(pixels(20), Axis::X);
  t.ui(parent).set_desired_padding(pixels(20), Axis::Y);

  // Child should fit inside padding
  auto &child = t.make_ui(percent(1.0f), percent(1.0f));
  t.add_child(root, parent);
  t.add_child(parent, child);
  t.run(root);

  // Parent rect: full 200x200 (absolute, no margin)
  auto rp = t.ui(parent).rect();
  CHECK_APPROX(rp.width, 200.f);
  CHECK_APPROX(rp.height, 200.f);

  // Child: 100% of parent's content area (200 - 40 padding = 160)
  auto rc = t.ui(child).rect();
  CHECK_APPROX(rc.width, 160.f - t.ui(child).computed_margin[Axis::X]);
  CHECK_APPROX(rc.height, 160.f - t.ui(child).computed_margin[Axis::Y]);
}

// ---------------------------------------------------------------------------
// Absolute child doesn't contribute to parent children() sizing
// ---------------------------------------------------------------------------
TEST(absolute_excluded_from_children_sizing) {
  TestLayout t;
  auto &root = t.make_ui(pixels(800), pixels(600));
  t.ui(root).set_flex_direction(FlexDirection::Column);

  // Parent uses children() sizing
  auto &parent = t.make_ui(children(), children());
  t.ui(parent).set_flex_direction(FlexDirection::Column);

  // Flow child: 100x50 — this should determine parent size
  auto &flow_child = t.make_ui(pixels(100), pixels(50));
  t.add_child(parent, flow_child);

  // Absolute child: 300x300 — should NOT inflate parent size
  auto &abs_child = t.make_ui(pixels(300), pixels(300));
  t.ui(abs_child).make_absolute();
  t.add_child(parent, abs_child);

  t.add_child(root, parent);
  t.run(root);

  // Parent should be sized to flow child only (100x50), not 300x300
  CHECK_APPROX(t.ui(parent).computed[Axis::X], 100.f);
  CHECK_APPROX(t.ui(parent).computed[Axis::Y], 50.f);
}

// ---------------------------------------------------------------------------
// Multiple absolute children with different margins don't interfere
// ---------------------------------------------------------------------------
TEST(multiple_absolute_independent_margins) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));

  auto &a = t.make_ui(pixels(80), pixels(80));
  t.ui(a).make_absolute();
  t.ui(a).set_desired_margin(Margin{
      .top = pixels(10), .bottom = pixels(0),
      .left = pixels(10), .right = pixels(0),
  });

  auto &b = t.make_ui(pixels(80), pixels(80));
  t.ui(b).make_absolute();
  t.ui(b).set_desired_margin(Margin{
      .top = pixels(100), .bottom = pixels(0),
      .left = pixels(200), .right = pixels(0),
  });

  t.add_child(root, a);
  t.add_child(root, b);
  t.run(root);

  auto ra = t.ui(a).rect();
  CHECK_APPROX(ra.x, 10.f);
  CHECK_APPROX(ra.y, 10.f);
  CHECK_APPROX(ra.width, 80.f);
  CHECK_APPROX(ra.height, 80.f);

  auto rb = t.ui(b).rect();
  CHECK_APPROX(rb.x, 200.f);
  CHECK_APPROX(rb.y, 100.f);
  CHECK_APPROX(rb.width, 80.f);
  CHECK_APPROX(rb.height, 80.f);
}

// ---------------------------------------------------------------------------
// Absolute child with screen_pct sizing resolves against screen
// ---------------------------------------------------------------------------
TEST(absolute_screen_pct_sizing) {
  TestLayout t;
  // Screen is 1280x720 (TestLayout default)
  auto &root = t.make_ui(pixels(400), pixels(400));

  auto &child = t.make_ui(screen_pct(0.5f), screen_pct(0.25f));
  t.ui(child).make_absolute();
  t.add_child(root, child);
  t.run(root);

  auto r = t.ui(child).rect();
  CHECK_APPROX(r.width, 640.f);  // 50% of 1280
  CHECK_APPROX(r.height, 180.f); // 25% of 720
}

// ---------------------------------------------------------------------------
// Absolute + flow siblings: absolute doesn't affect flow stacking
// ---------------------------------------------------------------------------
TEST(absolute_and_flow_siblings_stacking) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);

  auto &flow1 = t.make_ui(pixels(400), pixels(60));
  auto &abs1 = t.make_ui(pixels(100), pixels(100));
  t.ui(abs1).make_absolute();
  t.ui(abs1).set_desired_margin(Margin{
      .top = pixels(50), .bottom = pixels(0),
      .left = pixels(50), .right = pixels(0),
  });
  auto &flow2 = t.make_ui(pixels(400), pixels(60));
  auto &flow3 = t.make_ui(pixels(400), pixels(60));

  t.add_child(root, flow1);
  t.add_child(root, abs1);
  t.add_child(root, flow2);
  t.add_child(root, flow3);
  t.run(root);

  // Flow children stack normally, ignoring the absolute child
  CHECK_APPROX(t.ui(flow1).computed_rel[Axis::Y], 0.f);
  CHECK_APPROX(t.ui(flow2).computed_rel[Axis::Y], 60.f);
  CHECK_APPROX(t.ui(flow3).computed_rel[Axis::Y], 120.f);

  // Absolute child is positioned by its margin
  auto ra = t.ui(abs1).rect();
  CHECK_APPROX(ra.x, 50.f);
  CHECK_APPROX(ra.y, 50.f);
}

// ============================================================================
// Adaptive Scaling Mode tests
// ============================================================================

// ---------------------------------------------------------------------------
// Proportional mode (default): pixels() are not affected by ui_scale
// ---------------------------------------------------------------------------
TEST(proportional_mode_ignores_ui_scale) {
  TestLayout t;
  t.ui_scale = 2.0f;  // Double scale — should NOT affect proportional mode
  auto &root = t.make_ui(pixels(400), pixels(300));
  t.run(root);

  CHECK_APPROX(t.ui(root).computed[Axis::X], 400.f);
  CHECK_APPROX(t.ui(root).computed[Axis::Y], 300.f);
}

// ---------------------------------------------------------------------------
// Adaptive mode: pixels() are multiplied by ui_scale
// ---------------------------------------------------------------------------
TEST(adaptive_mode_scales_pixels) {
  TestLayout t;
  t.ui_scale = 2.0f;
  auto &root = t.make_ui_adaptive(pixels(400), pixels(300));
  t.run(root);

  // 400 * 2.0 = 800, 300 * 2.0 = 600
  CHECK_APPROX(t.ui(root).computed[Axis::X], 800.f);
  CHECK_APPROX(t.ui(root).computed[Axis::Y], 600.f);
}

// ---------------------------------------------------------------------------
// Adaptive mode: ui_scale of 1.0 produces same result as proportional
// ---------------------------------------------------------------------------
TEST(adaptive_mode_scale_1_matches_proportional) {
  TestLayout t;
  t.ui_scale = 1.0f;
  auto &root = t.make_ui_adaptive(pixels(400), pixels(300));
  t.run(root);

  CHECK_APPROX(t.ui(root).computed[Axis::X], 400.f);
  CHECK_APPROX(t.ui(root).computed[Axis::Y], 300.f);
}

// ---------------------------------------------------------------------------
// Adaptive mode: screen_pct() is NOT affected by ui_scale
// ---------------------------------------------------------------------------
TEST(adaptive_mode_screen_pct_not_scaled) {
  TestLayout t;
  t.ui_scale = 2.0f;
  t.resolution = {1280, 720};
  auto &root = t.make_ui_adaptive(screen_pct(0.5f), screen_pct(0.5f));
  t.run(root);

  // screen_pct resolves against screen, not scaled
  CHECK_APPROX(t.ui(root).computed[Axis::X], 640.f);
  CHECK_APPROX(t.ui(root).computed[Axis::Y], 360.f);
}

// ---------------------------------------------------------------------------
// Adaptive mode: h720() (which is screen_pct) is NOT affected by ui_scale
// ---------------------------------------------------------------------------
TEST(adaptive_mode_h720_not_scaled) {
  TestLayout t;
  t.ui_scale = 1.5f;
  t.resolution = {1280, 720};
  // h720(100) == screen_pct(100/720). ScreenPercent resolves against screen
  // dimension for the *same* axis. For Y: 100/720 * 720 = 100. For X:
  // 100/720 * 1280 ≈ 177.78. Use h720 on height (Y) axis only.
  // Use w1280 for X axis: w1280(100) = screen_pct(100/1280) => 100/1280*1280=100
  auto &root = t.make_ui_adaptive(w1280(100.f), h720(50.f));
  t.run(root);

  // Neither should be affected by ui_scale (they're ScreenPercent, not Pixels)
  CHECK_APPROX(t.ui(root).computed[Axis::X], 100.f);
  CHECK_APPROX(t.ui(root).computed[Axis::Y], 50.f);
}

// ---------------------------------------------------------------------------
// Adaptive mode: percent() is NOT affected by ui_scale (resolves against parent)
// ---------------------------------------------------------------------------
TEST(adaptive_mode_percent_not_scaled) {
  TestLayout t;
  t.ui_scale = 2.0f;
  auto &root = t.make_ui_adaptive(pixels(400), pixels(400));
  auto &child = t.make_ui_adaptive(percent(0.5f), percent(0.5f));
  t.add_child(root, child);
  t.run(root);

  // Root: 400*2 = 800. Child at 50% of 800 = 400.
  CHECK_APPROX(t.ui(root).computed[Axis::X], 800.f);
  CHECK_APPROX(t.ui(child).computed[Axis::X], 400.f);
  CHECK_APPROX(t.ui(child).computed[Axis::Y], 400.f);
}

// ---------------------------------------------------------------------------
// Adaptive mode: padding in pixels scales with ui_scale
// ---------------------------------------------------------------------------
TEST(adaptive_mode_padding_scales) {
  TestLayout t;
  t.ui_scale = 2.0f;
  auto &root = t.make_ui_adaptive(pixels(200), pixels(200));
  t.ui(root).set_desired_padding(pixels(10), Axis::X);
  t.ui(root).set_desired_padding(pixels(10), Axis::Y);

  auto &child = t.make_ui_adaptive(percent(1.0f), percent(1.0f));
  t.add_child(root, child);
  t.run(root);

  // Root at 2x: 400x400. Padding: 10*2=20 per side, so 40 total.
  // Content area = 400-40 = 360
  CHECK_APPROX(t.ui(root).computed[Axis::X], 400.f);
  CHECK_APPROX(t.ui(child).computed[Axis::X], 360.f);
  CHECK_APPROX(t.ui(child).computed[Axis::Y], 360.f);
}

// ---------------------------------------------------------------------------
// Adaptive mode: margin in pixels scales with ui_scale
// ---------------------------------------------------------------------------
TEST(adaptive_mode_margin_scales) {
  TestLayout t;
  t.ui_scale = 2.0f;
  auto &root = t.make_ui_adaptive(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);

  auto &child = t.make_ui_adaptive(pixels(100), pixels(100));
  t.ui(child).set_desired_margin(Margin{.top = pixels(10)});
  t.add_child(root, child);
  t.run(root);

  // margin-top = 10 * 2 = 20. Child rect should be offset.
  auto r = t.ui(child).rect();
  CHECK_APPROX(r.y, 20.f);
}

// ---------------------------------------------------------------------------
// Adaptive mode: expand() still fills remaining space correctly
// ---------------------------------------------------------------------------
TEST(adaptive_mode_expand_fills_remaining) {
  TestLayout t;
  t.ui_scale = 1.5f;
  auto &root = t.make_ui_adaptive(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  auto &header = t.make_ui_adaptive(pixels(400), pixels(50));
  auto &body = t.make_ui_adaptive(pixels(400), expand());
  auto &footer = t.make_ui_adaptive(pixels(400), pixels(50));
  t.add_child(root, header);
  t.add_child(root, body);
  t.add_child(root, footer);
  t.run(root);

  // Root at 1.5x: 600. Header: 50*1.5=75. Footer: 50*1.5=75.
  // Body: 600 - 75 - 75 = 450
  CHECK_APPROX(t.ui(root).computed[Axis::Y], 600.f);
  CHECK_APPROX(t.ui(header).computed[Axis::Y], 75.f);
  CHECK_APPROX(t.ui(footer).computed[Axis::Y], 75.f);
  CHECK_APPROX(t.ui(body).computed[Axis::Y], 450.f);
}

// ---------------------------------------------------------------------------
// Adaptive mode: fractional ui_scale (0.75x shrinks)
// ---------------------------------------------------------------------------
TEST(adaptive_mode_shrink_scale) {
  TestLayout t;
  t.ui_scale = 0.75f;
  auto &root = t.make_ui_adaptive(pixels(400), pixels(400));
  t.run(root);

  // 400 * 0.75 = 300
  CHECK_APPROX(t.ui(root).computed[Axis::X], 300.f);
  CHECK_APPROX(t.ui(root).computed[Axis::Y], 300.f);
}

// ---------------------------------------------------------------------------
// Mixed modes: proportional child inside adaptive parent
// ---------------------------------------------------------------------------
TEST(mixed_modes_proportional_child_in_adaptive_parent) {
  TestLayout t;
  t.ui_scale = 2.0f;

  // Adaptive root: 400*2 = 800
  auto &root = t.make_ui_adaptive(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);

  // Proportional child: pixels(100) stays 100, not scaled
  auto &child = t.make_ui(pixels(100), pixels(100));
  t.add_child(root, child);
  t.run(root);

  CHECK_APPROX(t.ui(root).computed[Axis::X], 800.f);
  CHECK_APPROX(t.ui(child).computed[Axis::X], 100.f);  // Not scaled
  CHECK_APPROX(t.ui(child).computed[Axis::Y], 100.f);
}

// ---------------------------------------------------------------------------
// Mixed modes: adaptive child inside proportional parent
// ---------------------------------------------------------------------------
TEST(mixed_modes_adaptive_child_in_proportional_parent) {
  TestLayout t;
  t.ui_scale = 2.0f;

  // Proportional root: 400 stays 400
  auto &root = t.make_ui(pixels(400), pixels(400));
  t.ui(root).set_flex_direction(FlexDirection::Column);

  // Adaptive child: pixels(100)*2 = 200
  auto &child = t.make_ui_adaptive(pixels(100), pixels(100));
  t.add_child(root, child);
  t.run(root);

  CHECK_APPROX(t.ui(root).computed[Axis::X], 400.f);
  CHECK_APPROX(t.ui(child).computed[Axis::X], 200.f);  // Scaled by 2x
  CHECK_APPROX(t.ui(child).computed[Axis::Y], 200.f);
}

// ---------------------------------------------------------------------------
// Adaptive mode: min/max constraints scale with ui_scale
// ---------------------------------------------------------------------------
TEST(adaptive_mode_constraints_scale) {
  TestLayout t;
  t.ui_scale = 2.0f;

  auto &root = t.make_ui_adaptive(pixels(400), pixels(400));

  // Child at 10% = 80px (since root is 400*2=800), min = 200*2=400
  auto &child = t.make_ui_adaptive(percent(0.1f), pixels(100));
  t.ui(child).set_min_width(pixels(200));
  t.add_child(root, child);
  t.run(root);

  // 10% of 800 = 80, min(200*2)=400 -> should be 400
  CHECK(t.ui(child).computed[Axis::X] >= 399.f);
}

// ---------------------------------------------------------------------------
// Adaptive mode: max constraint prevents oversizing at scale
// ---------------------------------------------------------------------------
TEST(adaptive_mode_max_constraint_scales) {
  TestLayout t;
  t.ui_scale = 2.0f;

  auto &root = t.make_ui_adaptive(pixels(400), pixels(400));

  // Child at 100% = 800, max = 150*2 = 300
  auto &child = t.make_ui_adaptive(percent(1.0f), pixels(100));
  t.ui(child).set_max_width(pixels(150));
  t.add_child(root, child);
  t.run(root);

  CHECK(t.ui(child).computed[Axis::X] <= 301.f);
}

// ---------------------------------------------------------------------------
// Adaptive mode: complex layout scales uniformly
// ---------------------------------------------------------------------------
TEST(adaptive_mode_dashboard_layout) {
  TestLayout t;
  t.ui_scale = 1.5f;

  auto &root = t.make_ui_adaptive(pixels(800), pixels(600));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_flex_wrap(FlexWrap::NoWrap);

  auto &header = t.make_ui_adaptive(percent(1.0f), pixels(60));
  auto &body = t.make_ui_adaptive(percent(1.0f), expand());
  t.ui(body).set_flex_direction(FlexDirection::Row);
  t.ui(body).set_flex_wrap(FlexWrap::NoWrap);
  auto &footer = t.make_ui_adaptive(percent(1.0f), pixels(40));

  t.add_child(root, header);
  t.add_child(root, body);
  t.add_child(root, footer);

  auto &sidebar = t.make_ui_adaptive(pixels(200), percent(1.0f));
  auto &main = t.make_ui_adaptive(expand(), percent(1.0f));
  t.add_child(body, sidebar);
  t.add_child(body, main);

  t.run(root);

  // Root: 800*1.5=1200, 600*1.5=900
  CHECK_APPROX(t.ui(root).computed[Axis::X], 1200.f);
  CHECK_APPROX(t.ui(root).computed[Axis::Y], 900.f);

  // Header: 60*1.5=90
  CHECK_APPROX(t.ui(header).computed[Axis::Y], 90.f);
  // Footer: 40*1.5=60
  CHECK_APPROX(t.ui(footer).computed[Axis::Y], 60.f);
  // Body: 900-90-60=750
  CHECK_APPROX(t.ui(body).computed[Axis::Y], 750.f);

  // Sidebar: 200*1.5=300
  CHECK_APPROX(t.ui(sidebar).computed[Axis::X], 300.f);
  // Main: 1200-300=900
  CHECK_APPROX(t.ui(main).computed[Axis::X], 900.f);
}

// ---------------------------------------------------------------------------
// ScalingMode enum: verify default is Proportional
// ---------------------------------------------------------------------------
TEST(scaling_mode_default_is_proportional) {
  TestLayout t;
  auto &root = t.make_ui(pixels(100), pixels(100));
  CHECK(t.ui(root).resolved_scaling_mode == ScalingMode::Proportional);
}

// ---------------------------------------------------------------------------
// LayoutInfo: basic breakpoint calculations
// ---------------------------------------------------------------------------
TEST(layout_info_proportional_mode) {
  // In proportional mode, logical dims equal screen dims
  auto info = LayoutInfo::make(1280.f, 720.f, 2.0f, ScalingMode::Proportional);
  CHECK_APPROX(info.logical_w, 1280.f);
  CHECK_APPROX(info.logical_h, 720.f);
  CHECK(info.is_wide());
  CHECK(!info.is_narrow());
  CHECK(!info.is_short());
}

TEST(layout_info_adaptive_mode) {
  // In adaptive mode, logical dims = screen / ui_scale
  auto info = LayoutInfo::make(1280.f, 720.f, 2.0f, ScalingMode::Adaptive);
  CHECK_APPROX(info.logical_w, 640.f);  // 1280 / 2
  CHECK_APPROX(info.logical_h, 360.f);  // 720 / 2
  CHECK(!info.is_wide());
  CHECK(info.is_narrow());  // 640 < 800
  CHECK(info.is_short());   // 360 < 600
}

TEST(layout_info_adaptive_narrow_breakpoint) {
  auto info = LayoutInfo::make(800.f, 600.f, 1.5f, ScalingMode::Adaptive);
  // logical_w = 800 / 1.5 ≈ 533
  CHECK(info.is_narrow());  // 533 < 800
}

TEST(layout_info_adaptive_wide_breakpoint) {
  auto info = LayoutInfo::make(1920.f, 1080.f, 1.5f, ScalingMode::Adaptive);
  // logical_w = 1920 / 1.5 = 1280
  CHECK(info.is_wide());  // 1280 >= 1200
}

// ---------------------------------------------------------------------------
// Adaptive mode: screen_pct margin is NOT scaled by ui_scale
// ---------------------------------------------------------------------------
TEST(adaptive_mode_screen_pct_margin_not_scaled) {
  TestLayout t;
  t.ui_scale = 2.0f;
  t.resolution = {1280, 720};

  auto &root = t.make_ui_adaptive(screen_pct(1.0f), screen_pct(1.0f));
  t.ui(root).set_flex_direction(FlexDirection::Column);

  auto &child = t.make_ui_adaptive(pixels(100), pixels(100));
  t.ui(child).set_desired_margin(Margin{.top = screen_pct(0.1f)});
  t.add_child(root, child);
  t.run(root);

  // screen_pct(0.1) margin-top = 0.1 * 720 = 72 (not scaled by ui_scale)
  auto r = t.ui(child).rect();
  CHECK_APPROX(r.y, 72.f);
}

// ---------------------------------------------------------------------------
// Adaptive mode: asymmetric padding in pixels all scale
// ---------------------------------------------------------------------------
TEST(adaptive_mode_asymmetric_padding_scales) {
  TestLayout t;
  t.ui_scale = 2.0f;

  auto &root = t.make_ui_adaptive(pixels(300), pixels(300));
  t.ui(root).set_desired_padding(Padding{
      .top = pixels(10),
      .left = pixels(20),
      .bottom = pixels(30),
      .right = pixels(40),
  });

  auto &child = t.make_ui_adaptive(percent(1.0f), percent(1.0f));
  t.add_child(root, child);
  t.run(root);

  // Root: 300*2 = 600
  // Horizontal padding: (20+40)*2 = 120, content = 600-120 = 480
  // Vertical padding: (10+30)*2 = 80, content = 600-80 = 520
  CHECK_APPROX(t.ui(root).computed[Axis::X], 600.f);
  CHECK_APPROX(t.ui(child).computed[Axis::X], 480.f);
  CHECK_APPROX(t.ui(child).computed[Axis::Y], 520.f);
}

// ===========================================================================
// Gap: spacing between children (CSS gap equivalent)
// ===========================================================================

// ---------------------------------------------------------------------------
// Gap: basic gap between children in column
// ---------------------------------------------------------------------------
TEST(gap_column_basic) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(300));
  t.ui(root).desired_gap = pixels(10.f);

  auto &a = t.make_ui(pixels(400), pixels(50));
  auto &b = t.make_ui(pixels(400), pixels(50));
  auto &c = t.make_ui(pixels(400), pixels(50));
  t.add_child(root, a);
  t.add_child(root, b);
  t.add_child(root, c);
  t.run(root);

  CHECK_APPROX(t.ui(a).computed_rel[Axis::Y], 0.f);
  CHECK_APPROX(t.ui(b).computed_rel[Axis::Y], 60.f);   // 50 + 10 gap
  CHECK_APPROX(t.ui(c).computed_rel[Axis::Y], 120.f);  // 50 + 10 + 50 + 10
}

// ---------------------------------------------------------------------------
// Gap: basic gap in row
// ---------------------------------------------------------------------------
TEST(gap_row_basic) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(100));
  t.ui(root).flex_direction = FlexDirection::Row;
  t.ui(root).desired_gap = pixels(8.f);

  auto &a = t.make_ui(pixels(100), pixels(100));
  auto &b = t.make_ui(pixels(100), pixels(100));
  t.add_child(root, a);
  t.add_child(root, b);
  t.run(root);

  CHECK_APPROX(t.ui(a).computed_rel[Axis::X], 0.f);
  CHECK_APPROX(t.ui(b).computed_rel[Axis::X], 108.f);  // 100 + 8
}

// ---------------------------------------------------------------------------
// Gap: expand divides remaining space after gap
// ---------------------------------------------------------------------------
TEST(gap_with_expand) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(300));
  t.ui(root).desired_gap = pixels(20.f);

  auto &fixed = t.make_ui(pixels(400), pixels(50));
  auto &exp = t.make_ui(pixels(400), expand());
  t.add_child(root, fixed);
  t.add_child(root, exp);
  t.run(root);

  // expand gets: 300 - 50 (fixed) - 20 (gap) = 230
  CHECK_APPROX(t.ui(exp).computed[Axis::Y], 230.f);
}

// ---------------------------------------------------------------------------
// Gap: single child — no gap applied
// ---------------------------------------------------------------------------
TEST(gap_single_child) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(300));
  t.ui(root).desired_gap = pixels(10.f);

  auto &a = t.make_ui(pixels(400), pixels(50));
  t.add_child(root, a);
  t.run(root);

  CHECK_APPROX(t.ui(a).computed_rel[Axis::Y], 0.f);
  CHECK_APPROX(t.ui(a).computed[Axis::Y], 50.f);
}

// ===========================================================================
// Absolute positioning: children of absolute parent inherit position
// ===========================================================================

// ---------------------------------------------------------------------------
// Absolute parent with flow children: children inherit parent position
// ---------------------------------------------------------------------------
TEST(absolute_parent_children_inherit_position) {
  TestLayout t;
  auto &root = t.make_ui(pixels(800), pixels(600));

  auto &abs_parent = t.make_ui(pixels(200), pixels(200));
  t.ui(abs_parent).make_absolute();
  t.ui(abs_parent).absolute_pos_x = 100.f;
  t.ui(abs_parent).absolute_pos_y = 50.f;

  auto &child = t.make_ui(pixels(80), pixels(40));
  t.add_child(root, abs_parent);
  t.add_child(abs_parent, child);
  t.run(root);

  // Child should be at parent's position (100, 50) + child's own offset (0, 0)
  CHECK_APPROX(t.ui(child).computed_rel[Axis::X], 100.f);
  CHECK_APPROX(t.ui(child).computed_rel[Axis::Y], 50.f);
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
