// Adding content above the fold must not move what the viewport is showing.
//
// scroll_offset counts pixels from the top of the content, so prepending rows
// leaves the same pixel showing older content and the view appears to jump to
// the top. Anchoring follows a pinned child instead.
#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include "ui_test_harness.h"

#include <cstdio>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const char *what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s\n", what);
  }
}

static bool near(float a, float b) { return std::fabs(a - b) < 0.51f; }

constexpr float ROW_H = 20.f;

// Build a scrolling column of `count` rows and measure it, the way the update
// pass does.
static void build_and_measure(ImmTestHarness &h, size_t first_item,
                              size_t count, bool anchor) {
  h.begin_frame();
  auto &ctx = h.context();
  auto view = imm::div(
      ctx, imm::mk(h.root()),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(200.f), pixels(100.f)})
          .with_overflow(Overflow::Scroll, Axis::Y)
          .with_flex_direction(FlexDirection::Column)
          .with_debug_name("anchored_view"));
  view.ent().get<HasScrollView>().anchor_scroll = anchor;
  // Keyed by item, not by position, so lowering first_item puts genuinely new
  // rows in front of the existing ones rather than appending.
  for (size_t i = first_item; i < first_item + count; i++) {
    imm::div(ctx, imm::mk(view.ent(), static_cast<EntityID>(i)),
             ComponentConfig{}
                 .with_size(ComponentSize{percent(1.f), pixels(ROW_H)})
                 .with_debug_name("anchor_row"));
  }
  h.layout_only();

  MeasureScrollViews measure;
  measure.for_each_with(view.ent(), view.ent().get<HasScrollView>(),
                        view.ent().get<UIComponent>(), 0.f);
}

static HasScrollView &view_of(ImmTestHarness &h) {
  for (const auto &e : h.coll.get_entities())
    if (e && e->has<HasScrollView>())
      return e->get<HasScrollView>();
  static HasScrollView dummy;
  return dummy;
}

// Rows are added at the top, so the offset has to grow by their height for the
// same row to stay under the same pixel.
static void test_prepending_holds_the_view_still() {
  ImmTestHarness h;
  build_and_measure(h, 5, 20, /*anchor=*/true);

  auto &sv = view_of(h);
  sv.scroll_offset.y = 100.f; // showing row 5 at the top
  sv.scroll_target.y = 100.f;
  sv.last_eased_offset = sv.scroll_offset;
  build_and_measure(h, 5, 20, true); // settle the anchor at this offset

  const float before = view_of(h).scroll_offset.y;
  // Five more rows above the fold.
  build_and_measure(h, 0, 25, true);
  const float after = view_of(h).scroll_offset.y;

  check(near(after - before, 5 * ROW_H),
        "offset follows the rows inserted above it");
}

// Off by default, and off means the old behaviour: the view jumps.
static void test_without_anchoring_the_view_jumps() {
  ImmTestHarness h;
  build_and_measure(h, 5, 20, /*anchor=*/false);

  auto &sv = view_of(h);
  sv.scroll_offset.y = 100.f;
  sv.scroll_target.y = 100.f;
  sv.last_eased_offset = sv.scroll_offset;
  build_and_measure(h, 5, 20, false);

  const float before = view_of(h).scroll_offset.y;
  build_and_measure(h, 0, 25, false);
  check(near(view_of(h).scroll_offset.y, before),
        "unanchored offset does not move, so the content under it changes");
}

// Growth below the fold is not growth above it, and must not move anything.
static void test_appending_does_not_move_the_view() {
  ImmTestHarness h;
  build_and_measure(h, 5, 20, /*anchor=*/true);

  auto &sv = view_of(h);
  sv.scroll_offset.y = 60.f;
  sv.scroll_target.y = 60.f;
  sv.last_eased_offset = sv.scroll_offset;
  build_and_measure(h, 5, 20, true);

  const float before = view_of(h).scroll_offset.y;
  // The extra rows go on the end, below what is on screen.
  build_and_measure(h, 5, 30, true);
  check(near(view_of(h).scroll_offset.y, before),
        "appending below the fold leaves the offset alone");
}

// A frame where the screen is not built must not reset the position
// (hanabi #163): children are cleared for every widget each frame, so
// measuring an unbuilt view reads zero content and the clamp zeroes a
// live offset.
static void test_unbuilt_frame_keeps_the_offset() {
  ImmTestHarness h;
  build_and_measure(h, 0, 20, /*anchor=*/false);

  auto &sv = view_of(h);
  sv.scroll_offset.y = 100.f;
  sv.scroll_target.y = 100.f;
  sv.last_eased_offset = sv.scroll_offset;
  build_and_measure(h, 0, 20, false);
  check(near(view_of(h).scroll_offset.y, 100.f), "settled offset is 100");
  check(near(view_of(h).content_size.y, 20 * ROW_H),
        "content measured at full height");

  Entity *view_ent = nullptr;
  for (const auto &e : h.coll.get_entities())
    if (e && e->has<HasScrollView>())
      view_ent = e.get();
  check(view_ent != nullptr, "found the view entity");
  if (!view_ent) return;
  view_ent->get<UIComponent>().children.clear();
  MeasureScrollViews measure;
  measure.for_each_with(*view_ent, view_ent->get<HasScrollView>(),
                        view_ent->get<UIComponent>(), 0.f);
  check(near(view_of(h).scroll_offset.y, 100.f),
        "an unbuilt frame keeps the offset");
  check(near(view_of(h).content_size.y, 20 * ROW_H),
        "an unbuilt frame keeps the measured content size");
}

// scroll_to_bottom / scroll_to_top (hanabi #476): one call moves offset,
// target and last_eased together, so the next ease does not drag the view
// back toward the stale target.
static void test_scroll_to_ends() {
  HasScrollView sv;
  sv.viewport_size = Vector2Type{200.f, 100.f};
  sv.content_size = Vector2Type{200.f, 1000.f};
  sv.scroll_to_bottom();
  check(near(sv.scroll_offset.y, 900.f), "end: offset at max");
  check(near(sv.scroll_target.y, 900.f), "end: target at max");
  check(near(sv.last_eased_offset.y, 900.f), "end: last eased at max");
  sv.ease_scroll(0.016f);
  check(near(sv.scroll_offset.y, 900.f), "end: ease does not pull it back");
  sv.scroll_to_top();
  check(near(sv.scroll_offset.y, 0.f) && near(sv.scroll_target.y, 0.f) &&
            near(sv.last_eased_offset.y, 0.f),
        "start: all three at zero");

  HasScrollView unmeasured;
  unmeasured.scroll_to_bottom();
  unmeasured.viewport_size = Vector2Type{200.f, 100.f};
  unmeasured.content_size = Vector2Type{200.f, 1000.f};
  unmeasured.clamp_scroll();
  check(near(unmeasured.scroll_offset.y, 900.f),
        "end before measuring clamps to max once measured");
}

// The horizontal pair, through the same public interface: measured max,
// the unmeasured sentinel, and holding across several ease frames.
static void test_scroll_to_ends_horizontal() {
  HasScrollView sv;
  sv.viewport_size = Vector2Type{200.f, 100.f};
  sv.content_size = Vector2Type{1000.f, 100.f};
  sv.scroll_to_right();
  check(near(sv.scroll_offset.x, 800.f), "right: offset at max");
  check(near(sv.scroll_target.x, 800.f), "right: target at max");
  check(near(sv.last_eased_offset.x, 800.f), "right: last eased at max");
  check(near(sv.scroll_offset.y, 0.f) && near(sv.scroll_target.y, 0.f),
        "right: vertical axis untouched");
  for (int frame = 0; frame < 5; frame++)
    sv.ease_scroll(0.016f);
  check(near(sv.scroll_offset.x, 800.f) && near(sv.scroll_target.x, 800.f) &&
            near(sv.last_eased_offset.x, 800.f),
        "right: holds across ease frames");
  sv.scroll_to_left();
  check(near(sv.scroll_offset.x, 0.f) && near(sv.scroll_target.x, 0.f) &&
            near(sv.last_eased_offset.x, 0.f),
        "left: all three at zero");
  for (int frame = 0; frame < 5; frame++)
    sv.ease_scroll(0.016f);
  check(near(sv.scroll_offset.x, 0.f), "left: holds across ease frames");

  HasScrollView unmeasured;
  unmeasured.scroll_to_right();
  check(near(unmeasured.scroll_offset.x, 1e9f) &&
            near(unmeasured.scroll_target.x, 1e9f) &&
            near(unmeasured.last_eased_offset.x, 1e9f),
        "right before measuring: all three at the sentinel");
  unmeasured.viewport_size = Vector2Type{200.f, 100.f};
  unmeasured.content_size = Vector2Type{1000.f, 100.f};
  unmeasured.clamp_scroll();
  check(near(unmeasured.scroll_offset.x, 800.f) &&
            near(unmeasured.scroll_target.x, 800.f),
        "right before measuring clamps to max once measured");
  for (int frame = 0; frame < 5; frame++)
    unmeasured.ease_scroll(0.016f);
  check(near(unmeasured.scroll_offset.x, 800.f),
        "right: clamped end holds across ease frames");
}

int main() {
  printf("Running scroll anchor tests...\n\n");
  printf("  prepending_holds_the_view_still\n");
  test_prepending_holds_the_view_still();
  printf("  without_anchoring_the_view_jumps\n");
  test_without_anchoring_the_view_jumps();
  printf("  appending_does_not_move_the_view\n");
  test_appending_does_not_move_the_view();
  printf("  unbuilt_frame_keeps_the_offset\n");
  test_unbuilt_frame_keeps_the_offset();
  printf("  scroll_to_ends\n");
  test_scroll_to_ends();
  printf("  scroll_to_ends_horizontal\n");
  test_scroll_to_ends_horizontal();

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("SOME TESTS FAILED\n");
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
