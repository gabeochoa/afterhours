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
constexpr float VIEW_H = 100.f;

static void build_view(ImmTestHarness &h, EntityID *view_id, EntityID *row_id) {
  h.begin_frame();
  auto &ctx = h.context();
  auto view = imm::div(
      ctx, imm::mk(h.root()),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(200.f), pixels(VIEW_H)})
          .with_overflow(Overflow::Scroll, Axis::Y)
          .with_flex_direction(FlexDirection::Column)
          .with_debug_name("scroll_to_view"));
  for (size_t i = 0; i < 20; i++) {
    imm::div(ctx, imm::mk(view.ent(), static_cast<EntityID>(i)),
             ComponentConfig{}
                 .with_size(ComponentSize{percent(1.f), pixels(ROW_H)})
                 .with_debug_name("scroll_to_row"));
  }
  h.layout_only();

  MeasureScrollViews measure;
  measure.for_each_with(view.ent(), view.ent().get<HasScrollView>(),
                        view.ent().get<UIComponent>(), 0.f);
  if (view_id)
    *view_id = view.ent().id;
  if (row_id) {
    const auto &kids = view.ent().get<UIComponent>().children;
    for (size_t i = 0; i < kids.size() && i < 20; i++)
      row_id[i] = kids[i];
  }
}

// Off-screen child: the offset moves by the exact smallest amount showing it.
static void test_entity_offscreen_exact_offset() {
  ImmTestHarness h;
  EntityID view_id = -1;
  EntityID row[20] = {};
  build_view(h, &view_id, row);

  auto &sv = UICollectionHolder::getEntityForID(view_id).asE().template get<HasScrollView>();
  check(near(sv.content_size.y, 400.f), "content measured at 400");
  check(near(sv.scroll_offset.y, 0.f), "starts at the top");

  scroll_to_entity(UICollectionHolder::getEntityForID(row[10]).asE());
  check(near(sv.scroll_offset.y, 120.f), "offset moves to exactly 120");
  check(near(sv.scroll_target.y, 120.f), "target follows the offset");
  check(near(sv.last_eased_offset.y, 120.f), "last eased follows the offset");
  sv.ease_scroll(0.016f);
  check(near(sv.scroll_offset.y, 120.f), "ease holds the new offset");

  scroll_to_entity(UICollectionHolder::getEntityForID(row[0]).asE());
  check(near(sv.scroll_offset.y, 0.f), "scrolls back to exactly 0");
}

static void test_entity_already_visible_noop() {
  ImmTestHarness h;
  EntityID view_id = -1;
  EntityID row[20] = {};
  build_view(h, &view_id, row);

  auto &sv = UICollectionHolder::getEntityForID(view_id).asE().template get<HasScrollView>();
  sv.scroll_to(Vector2Type{0.f, 50.f});
  scroll_to_entity(UICollectionHolder::getEntityForID(row[3]).asE());
  check(near(sv.scroll_offset.y, 50.f), "visible child leaves the offset alone");
  check(near(sv.scroll_target.y, 50.f), "visible child leaves the target alone");
}

static void test_clamps_at_extents() {
  ImmTestHarness h;
  EntityID view_id = -1;
  EntityID row[20] = {};
  build_view(h, &view_id, row);

  auto &sv = UICollectionHolder::getEntityForID(view_id).asE().template get<HasScrollView>();
  const float max_y = 400.f - VIEW_H;

  sv.scroll_to(Vector2Type{0.f, 99999.f});
  check(near(sv.scroll_offset.y, max_y), "offset past the end clamps to max");
  check(near(sv.scroll_target.y, max_y), "target clamped with the offset");
  check(near(sv.last_eased_offset.y, max_y), "last eased clamped with the offset");
  for (int frame = 0; frame < 5; frame++)
    sv.ease_scroll(0.016f);
  check(near(sv.scroll_offset.y, max_y), "clamped end holds across ease frames");

  sv.scroll_to(Vector2Type{0.f, -50.f});
  check(near(sv.scroll_offset.y, 0.f), "negative offset clamps to 0");

  scroll_to_entity(UICollectionHolder::getEntityForID(row[19]).asE());
  check(near(sv.scroll_offset.y, max_y), "last row lands exactly on the max");
  check(sv.scroll_offset.y <= max_y + 0.51f, "never past the max");
}

static void test_offset_both_axes() {
  HasScrollView sv;
  sv.viewport_size = Vector2Type{200.f, 100.f};
  sv.content_size = Vector2Type{1000.f, 1000.f};
  sv.scroll_to(Vector2Type{250.f, 150.f});
  check(near(sv.scroll_offset.x, 250.f) && near(sv.scroll_target.x, 250.f) &&
            near(sv.last_eased_offset.x, 250.f),
        "x: all three at 250");
  check(near(sv.scroll_offset.y, 150.f) && near(sv.scroll_target.y, 150.f) &&
            near(sv.last_eased_offset.y, 150.f),
        "y: all three at 150");
  for (int frame = 0; frame < 5; frame++)
    sv.ease_scroll(0.016f);
  check(near(sv.scroll_offset.x, 250.f) && near(sv.scroll_offset.y, 150.f),
        "offset holds across ease frames");

  HasScrollView unmeasured;
  unmeasured.scroll_to(Vector2Type{0.f, 99999.f});
  check(near(unmeasured.scroll_offset.y, 99999.f),
        "unmeasured: raw value until measured");
  unmeasured.viewport_size = Vector2Type{200.f, 100.f};
  unmeasured.content_size = Vector2Type{200.f, 1000.f};
  unmeasured.clamp_scroll();
  check(near(unmeasured.scroll_offset.y, 900.f),
        "unmeasured: clamps to max once measured");
}

int main() {
  printf("Running scroll_to tests...\n\n");
  printf("  entity_offscreen_exact_offset\n");
  test_entity_offscreen_exact_offset();
  printf("  entity_already_visible_noop\n");
  test_entity_already_visible_noop();
  printf("  clamps_at_extents\n");
  test_clamps_at_extents();
  printf("  offset_both_axes\n");
  test_offset_both_axes();

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("SOME TESTS FAILED\n");
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
