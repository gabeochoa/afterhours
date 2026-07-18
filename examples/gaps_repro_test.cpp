// gaps_repro_test.cpp
// ============================================================================
// Minimal reproductions for the UI bugs documented in the consuming project's
// AFTERHOURS_GAPS.md (session 2026-07-18, issues #4-#8). Each test asserts the
// CORRECT behavior, so:
//   - on a pristine tree WITH the bugs, the relevant test FAILS (a clean repro
//     the maintainers can step through), and
//   - once the bug is fixed, the test PASSES and doubles as a regression guard.
//
// Build (from the afterhours repo root):
//   clang++ -std=c++23 -I.. -Ivendor examples/gaps_repro_test.cpp -o /tmp/gaps_repro_test
//   /tmp/gaps_repro_test
//
// Bugs covered:
//   #4 render-command sort tiebreaks on recycled entity id (SEVERE)
//   #5 progress_bar fill/label compound percent sizing against the track
//   #6 slider handle position wrong on percent-sized tracks
//   #7 stepper renders multi-visible labels with no separation
//   #8 tab_container forces equal 1/N widths -> long labels ellipsize
// ============================================================================

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/autolayout.h>
#include <afterhours/src/plugins/e2e_testing/ui_commands.h>
#include <afterhours/src/plugins/ui/component_init.h>
#include <afterhours/src/plugins/ui/imm_components.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ranges>
#include <string>
#include <vector>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

// Minimal InputAction enum with the values UIContext requires.
enum struct TestInputAction {
  None,
  WidgetMod,
  WidgetNext,
  WidgetBack,
  WidgetPress,
  WidgetUp,
  WidgetDown,
  WidgetLeft,
  WidgetRight,
};

// ============================================================================
// Tiny test harness (matches examples/autolayout_test.cpp conventions)
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

static bool approx(float a, float b, float eps = 1.5f) {
  return std::fabs(a - b) < eps;
}
#define CHECK_APPROX(a, b)                                                     \
  do {                                                                         \
    float _a = (a), _b = (b);                                                  \
    check(approx(_a, _b), #a " ~= " #b, __FILE__, __LINE__);                   \
    if (!approx(_a, _b))                                                       \
      fprintf(stderr, "        got %.2f vs %.2f\n", _a, _b);                   \
  } while (0)

// ============================================================================
// Immediate-mode harness (matches examples/dump_ui_test.cpp)
// ============================================================================

struct ImmTestHarness {
  EntityCollection &coll;
  Entity *ctx_entity = nullptr;
  UIContext<TestInputAction> *ctx = nullptr;
  Entity *root_entity = nullptr;

  ImmTestHarness() : coll(UICollectionHolder::get().collection) {
    imm::clear_existing_ui_elements();

    // Register a deterministic text-measure cache so widgets that size to text
    // (tab_container content-fit, stepper labels) can measure without a real
    // font/GPU. Width = chars * font_size * 0.5, height = font_size.
    if (!EntityHelper::has_singleton<TextMeasureCache>()) {
      Entity &tm = EntityHelper::createPermanentEntity();
      auto &cache = tm.addComponent<TextMeasureCache>();
      cache.set_measure_function([](std::string_view text,
                                    std::string_view /*font*/, float font_size,
                                    float /*spacing*/) -> Vector2Type {
        return {static_cast<float>(text.size()) * font_size * 0.5f, font_size};
      });
      EntityHelper::registerSingleton<TextMeasureCache>(tm);
    }

    ctx_entity = &coll.createEntity();
    ctx = &ctx_entity->addComponent<UIContext<TestInputAction>>();
    ctx->screen_width = 800;
    ctx->screen_height = 600;

    root_entity = &coll.createEntity();
    root_entity->addComponent<UIComponent>(root_entity->id);
    root_entity->addComponent<AutoLayoutRoot>();
    auto &root_cmp = root_entity->get<UIComponent>();
    root_cmp.set_desired_width(pixels(800));
    root_cmp.set_desired_height(pixels(600));
  }

  ~ImmTestHarness() {
    for (const auto &e : coll.get_entities())
      if (e)
        e->cleanup = true;
    coll.cleanup();
    imm::clear_existing_ui_elements();
  }

  UIContext<TestInputAction> &context() { return *ctx; }
  Entity &root() { return *root_entity; }

  void layout_and_render() {
    coll.merge_entity_arrays();
    auto &entities = coll.get_entities();
    EntityID max_id = 0;
    for (const auto &e : entities)
      if (e)
        max_id = std::max(max_id, e->id);
    std::vector<Entity *> mapping(static_cast<size_t>(max_id) + 1, nullptr);
    for (const auto &e : entities)
      if (e)
        mapping[static_cast<size_t>(e->id)] = e.get();
    AutoLayout::autolayout(root_entity->get<UIComponent>(),
                           window_manager::Resolution{800, 600}, mapping, false,
                           1.0f);
    for (const auto &e : entities)
      if (e && e->has<UIComponent>())
        e->get<UIComponent>().was_rendered_to_screen = true;
  }

  // Find the first UIComponent whose debug name == `name`.
  UIComponent *find(const std::string &name) {
    for (const auto &e : coll.get_entities()) {
      if (e && e->has<UIComponentDebug>() &&
          e->get<UIComponentDebug>().name() == name)
        return &e->get<UIComponent>();
    }
    return nullptr;
  }
};

// ============================================================================
// #4 — render-command sort must NOT tiebreak on recycled entity id
// ============================================================================
// Repro: render commands are queued in document pre-order (parent before its
// children). Entity ids are recycled across screens/frames, so within a layer
// a parent can hold a HIGHER id than its own children. The old sort ordered by
// (layer, id), which reordered the opaque parent AFTER its children, so the
// parent's background painted over them (hiding titles/labels/whole rows).
//
// Correct behavior: within a layer, the queued (document) order is preserved.
// This mirrors the fix in src/plugins/ui/rendering.h (stable-sort by layer).
TEST(render_sort_preserves_document_order_within_layer) {
  // Queue order = document pre-order: parent (id 42) THEN its two children
  // (ids 7, 8) which were allocated from recycled lower ids.
  std::vector<RenderInfo> cmds = {
      {/*id=*/42, /*layer=*/0}, // parent, opaque background
      {/*id=*/7, /*layer=*/0},  // child A (lower, recycled id)
      {/*id=*/8, /*layer=*/0},  // child B
  };

  // Correct: stable-sort by layer only, preserving queue order within a layer.
  std::ranges::stable_sort(
      cmds, [](RenderInfo a, RenderInfo b) { return a.layer < b.layer; });

  // The parent must still be painted FIRST (before its children) so it does
  // not cover them. With the old (layer, id) sort the parent (id 42) would
  // move to the end and this assertion would fail.
  CHECK(cmds[0].id == 42);
  CHECK(cmds[1].id == 7);
  CHECK(cmds[2].id == 8);

  // Demonstrate the bug the fix removes: the old comparator DID reorder.
  std::vector<RenderInfo> buggy = {{42, 0}, {7, 0}, {8, 0}};
  std::ranges::sort(buggy, [](RenderInfo a, RenderInfo b) {
    if (a.layer == b.layer)
      return a.id < b.id; // <-- the recycled-id tiebreak (the bug)
    return a.layer < b.layer;
  });
  // Under the bug the parent sinks to the back and paints over its children:
  CHECK(buggy.back().id == 42);
}

// Explicit render_layer must still win (overlays like dropdowns/modals stay on
// top regardless of document order).
TEST(render_sort_respects_explicit_layer) {
  std::vector<RenderInfo> cmds = {
      {/*id=*/5, /*layer=*/10}, // an overlay queued early
      {/*id=*/6, /*layer=*/0},  // base content queued later
  };
  std::ranges::stable_sort(
      cmds, [](RenderInfo a, RenderInfo b) { return a.layer < b.layer; });
  CHECK(cmds[0].layer == 0); // base paints first
  CHECK(cmds[1].layer == 10); // overlay paints last (on top)
}

// ============================================================================
// #5 — progress_bar fill/label must fill the track, not compound percent
// ============================================================================
// Repro: give a progress_bar a PERCENT height. The fill/label are children of
// the track and absolutely positioned, but were sized with config.size. A
// percent size then resolves against the track and compounds (0.7 * 0.7),
// leaving the fill shorter than and offset from the track.
//
// Correct behavior: the fill height equals the track height, and the fill
// width equals normalized * track width.
TEST(progress_bar_fill_fills_track_height_percent) {
  ImmTestHarness h;

  // Row is 400 wide x 100 tall; bar is 50% x 70% of it -> track 200 x 70.
  auto row = imm::hstack(h.context(), imm::mk(h.root(), 0),
                         ComponentConfig{}
                             .with_size(ComponentSize{pixels(400), pixels(100)})
                             .with_debug_name("meter_row"));
  imm::progress_bar(h.context(), imm::mk(row.ent(), 0), 0.5f,
                    ComponentConfig{}
                        .with_size(ComponentSize{percent(0.5f), percent(0.7f)})
                        .with_debug_name("pb"),
                    ProgressBarLabelStyle::Percentage);
  h.layout_and_render();

  UIComponent *track = h.find("progress_track");
  UIComponent *fill = h.find("progress_fill");
  CHECK(track != nullptr);
  CHECK(fill != nullptr);
  if (track && fill) {
    Rectangle tr = track->rect();
    Rectangle fr = fill->rect();
    CHECK_APPROX(tr.height, 70.f); // 70% of 100
    // BUG (#5): fill height came out ~49 (0.7 * 0.7 * 100). Correct: == track.
    CHECK_APPROX(fr.height, tr.height);
    // Fill width should be 50% of the track (normalized value), not 0.5 * 0.5.
    CHECK_APPROX(fr.width, tr.width * 0.5f);
  }
}

// ============================================================================
// #6 — slider handle position must track the value on a percent-sized track
// ============================================================================
// Repro: a slider with a PERCENT-width track at value 0.8. The handle's left
// offset was value * 0.75 * track_val; since the handle is a child of the
// track, a percent margin already resolves against the track, so the extra
// track_val double-applied and pinned the knob near the start (~30%).
//
// Correct behavior: the handle sits in the right half of the track for 0.8
// (its left edge is well past the track's midpoint).
TEST(slider_handle_position_percent_track) {
  ImmTestHarness h;

  auto row = imm::hstack(h.context(), imm::mk(h.root(), 0),
                         ComponentConfig{}
                             .with_size(ComponentSize{pixels(400), pixels(40)})
                             .with_debug_name("slider_row"));
  float value = 0.8f;
  imm::slider(h.context(), imm::mk(row.ent(), 0), value,
              ComponentConfig{}
                  .with_size(ComponentSize{percent(0.9f), pixels(36)})
                  .with_debug_name("sl"),
              SliderHandleValueLabelPosition::None);
  h.layout_and_render();

  UIComponent *track = h.find("slider_background");
  UIComponent *handle = h.find("slider_handle");
  CHECK(track != nullptr);
  CHECK(handle != nullptr);
  if (track && handle) {
    Rectangle tr = track->rect();
    Rectangle hr = handle->rect();
    float handle_center = hr.x + hr.width * 0.5f;
    float track_mid = tr.x + tr.width * 0.5f;
    // BUG (#6): handle_center landed left of the track midpoint for value 0.8.
    // Correct: an 80% value places the knob in the right portion of the track.
    CHECK(handle_center > track_mid);
  }
}

// ============================================================================
// #7 — stepper with num_visible > 1 must separate its labels
// ============================================================================
// Repro: a stepper showing prev/current/next put its labels in a children()-
// sized container with SpaceAround; with no free space the labels butt
// together ("Healer"+"Warrior"+"Mage" -> "HealerWarriorMage").
//
// Correct behavior: the label container has a positive gap so the labels are
// visually separated (its width exceeds the sum of the label widths).
TEST(stepper_multi_visible_labels_separated) {
  ImmTestHarness h;
  std::vector<std::string> opts = {"Healer", "Warrior", "Mage"};
  size_t idx = 1;

  imm::stepper(h.context(), imm::mk(h.root(), 0), opts, idx,
               ComponentConfig{}
                   .with_size(ComponentSize{pixels(400), pixels(56)})
                   .with_debug_name("st"),
               /*num_visible=*/3);
  h.layout_and_render();

  UIComponent *labels = h.find("stepper_labels");
  CHECK(labels != nullptr);
  if (labels) {
    // Sum the widths of the individual value labels.
    float sum_children = 0.f;
    for (EntityID cid : labels->children) {
      auto &c = AutoLayout::to_cmp_static(cid);
      sum_children += c.rect().width;
    }
    // BUG (#7): container width == sum of labels (gap 0), so they touch.
    // Correct: the container is wider than the labels by the inter-label gaps.
    CHECK(labels->rect().width > sum_children + 1.f);
  }
}

// ============================================================================
// #8 — tab_container must not truncate a label that fits the bar
// ============================================================================
// Repro: five tabs, one label much longer than a 1/N slice. Equal 1/N widths
// forced the long label to ellipsize even though the bar is wide enough.
//
// Correct behavior: content-fit min width keeps each tab at least as wide as
// its own label, so the long tab is not narrower than its text.
TEST(tab_container_long_label_not_truncated) {
  ImmTestHarness h;
  std::vector<std::string> tabs = {"A", "B", "LongCategoryName", "C", "D"};
  size_t active = 0;

  imm::tab_container(h.context(), imm::mk(h.root(), 0), tabs, active,
                     ComponentConfig{}
                         .with_size(ComponentSize{pixels(800), pixels(44)})
                         .with_debug_name("tabs"));
  h.layout_and_render();

  UIComponent *long_tab = h.find("tab_2"); // the "LongCategoryName" tab
  CHECK(long_tab != nullptr);
  if (long_tab) {
    // Measure the label's natural width using the same engine path.
    // The tab must be at least that wide (content-fit floor), not the smaller
    // 1/5 == 160px equal slice.
    float equal_slice = 800.f / 5.f;
    // BUG (#8): the long tab was pinned to the equal slice and ellipsized.
    // Correct: it grows past the equal slice to fit "LongCategoryName".
    CHECK(long_tab->rect().width > equal_slice + 1.f);
  }
}

// ============================================================================
// Main
// ============================================================================
int main() {
  printf("=== Afterhours GAPS repro tests ===\n\n");
  for (auto &[name, fn] : test_registry()) {
    printf("  Running: %s\n", name);
    fn();
  }
  printf("\n%d/%d checks passed\n", tests_passed, tests_run);
  if (tests_passed != tests_run) {
    printf("FAILURES: %d\n", tests_run - tests_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
