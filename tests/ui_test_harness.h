// ui_test_harness.h
// Shared test scaffolding for the immediate-mode UI regression tests
// (progress_bar_test, slider_test, stepper_test, tab_container_test, ...).
//
// Provides:
//   - a tiny TEST/CHECK/CHECK_APPROX framework (matches autolayout_test.cpp)
//   - ImmTestHarness: builds a UIContext + layout root, runs autolayout with a
//     deterministic text-measure stub (no font/GPU needed), and finds nodes by
//     debug name
//   - run_registered_tests(): the shared main-body runner
//
// Each test .cpp includes this header, declares TEST(...) cases, and defines a
// main() that returns run_registered_tests("<suite name>").
#pragma once

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/autolayout.h>
#include <afterhours/src/plugins/e2e_testing/ui_commands.h>
#include <afterhours/src/plugins/ui/component_init.h>
#include <afterhours/src/plugins/ui/imm_components.h>
#include <afterhours/src/plugins/ui/rendering.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace ui_test {

using namespace afterhours;
using namespace afterhours::ui;

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

// ---------------------------------------------------------------------------
// Tiny test framework
// ---------------------------------------------------------------------------
inline int &tests_run() {
  static int n = 0;
  return n;
}
inline int &tests_passed() {
  static int n = 0;
  return n;
}

struct TestEntry {
  const char *name;
  void (*fn)();
};
inline std::vector<TestEntry> &test_registry() {
  static std::vector<TestEntry> r;
  return r;
}
inline void register_test(const char *name, void (*fn)()) {
  test_registry().push_back({name, fn});
}

inline void check(bool cond, const char *expr, const char *file, int line) {
  tests_run()++;
  if (cond) {
    tests_passed()++;
  } else {
    fprintf(stderr, "  FAIL: %s  (%s:%d)\n", expr, file, line);
  }
}

inline bool approx(float a, float b, float eps = 1.5f) {
  return std::fabs(a - b) < eps;
}

inline int run_registered_tests(const char *suite) {
  printf("=== %s ===\n\n", suite);
  for (auto &[name, fn] : test_registry()) {
    printf("  Running: %s\n", name);
    fn();
  }
  printf("\n%d/%d checks passed\n", tests_passed(), tests_run());
  if (tests_passed() != tests_run()) {
    printf("FAILURES: %d\n", tests_run() - tests_passed());
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}

// ---------------------------------------------------------------------------
// Immediate-mode harness: UIContext + layout root, no font/GPU required.
// ---------------------------------------------------------------------------
struct ImmTestHarness {
  EntityCollection &coll;
  Entity *ctx_entity = nullptr;
  UIContext<TestInputAction> *ctx = nullptr;
  Entity *root_entity = nullptr;
  Entity *font_entity = nullptr;

  ImmTestHarness() : coll(UICollectionHolder::get().collection) {
    imm::clear_existing_ui_elements();

    // Deterministic text measurement so widgets that size to text
    // (tab_container content-fit, stepper labels) work without a real font.
    // width = chars * font_size * 0.5, height = font_size.
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

    // Register the resolution the harness actually lays out at. Without it,
    // code that resolves h720/screen_pct against this singleton silently falls
    // back to a 1280x720 baseline while the context and autolayout both say
    // 800x600, so tests exercise a size combination no real app would see.
    if (!EntityHelper::has_singleton<
            window_manager::ProvidesCurrentResolution>()) {
      Entity &res = EntityHelper::createPermanentEntity();
      res.addComponent<window_manager::ProvidesCurrentResolution>(
          window_manager::Resolution{800, 600});
      EntityHelper::registerSingleton<
          window_manager::ProvidesCurrentResolution>(res);
    }

#ifdef AFTER_HOURS_BACKEND_NONE
    // The renderer calls the backend's free measure_text rather than going
    // through TextMeasureCache, so it needs the SAME stub installed separately
    // or the two disagree. Without this the `none` backend returns {0,0} and
    // every "does the text fit" branch takes the it-fits path, which makes any
    // render assertion about truncation quietly vacuous.
    set_measure_text_fn([](const char *text, float font_size, float) {
      return Vector2Type{
          static_cast<float>(std::string_view(text ? text : "").size()) *
              font_size * 0.5f,
          font_size};
    });
#endif

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

  void layout_only() {
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

  // Lay out, then actually run the renderer and return what it drew.
  //
  // layout_only() deliberately stops before rendering, which left everything
  // in rendering.h -- ellipsis truncation, colour resolution, opacity, borders
  // -- with no coverage at all. This drives RenderImm for real against the
  // `none` backend, which records draw calls instead of touching a GPU, so a
  // test can assert on what was drawn.
  //
  // Only available in a TU built WITHOUT a real backend macro: with
  // AFTER_HOURS_USE_RAYLIB the draws go to raylib and nothing is recorded, so
  // this is compiled out rather than offered and quietly returning nothing.
  // Keep render-asserting suites out of RAYLIB_TESTS in tests/Makefile.
#ifdef AFTER_HOURS_BACKEND_NONE
  const std::vector<DrawCall> &render() {
    layout_only();

    if (!font_entity) {
      font_entity = &coll.createEntity();
      auto &fm = font_entity->addComponent<FontManager>();
      fm.load_font(UIComponent::DEFAULT_FONT, get_default_font());
    }

    clear_draw_calls();
    RenderImm<TestInputAction> renderer;
    renderer.for_each_with_derived(*root_entity, *ctx,
                                   font_entity->get<FontManager>(), 0.f);
    return draw_calls();
  }

  // Every recorded draw of a given op, in paint order.
  std::vector<DrawCall> drawn(const std::string &op) {
    std::vector<DrawCall> out;
    for (const auto &c : draw_calls())
      if (c.op == op)
        out.push_back(c);
    return out;
  }
#endif // AFTER_HOURS_BACKEND_NONE

  // First UIComponent whose debug name == `name` (nullptr if none).
  UIComponent *find(const std::string &name) {
    for (const auto &e : coll.get_entities()) {
      if (e && e->has<UIComponentDebug>() &&
          e->get<UIComponentDebug>().name() == name)
        return &e->get<UIComponent>();
    }
    return nullptr;
  }
};

} // namespace ui_test

// TEST/CHECK macros live at global scope for terse use in test files.
#define TEST(name)                                                             \
  static void test_##name();                                                   \
  struct Register_##name {                                                     \
    Register_##name() { ::ui_test::register_test(#name, test_##name); }        \
  } register_##name##_instance;                                                \
  static void test_##name()

#define CHECK(expr)                                                            \
  ::ui_test::check((expr), #expr, __FILE__, __LINE__)

#define CHECK_APPROX(a, b)                                                     \
  do {                                                                         \
    float _a = (a), _b = (b);                                                  \
    ::ui_test::check(::ui_test::approx(_a, _b), #a " ~= " #b, __FILE__,        \
                     __LINE__);                                                \
    if (!::ui_test::approx(_a, _b))                                            \
      fprintf(stderr, "        got %.2f vs %.2f\n", _a, _b);                   \
  } while (0)
