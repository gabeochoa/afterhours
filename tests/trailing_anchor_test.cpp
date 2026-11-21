#define FMT_HEADER_ONLY
#define AFTER_HOURS_USE_NONE
#include <fmt/format.h>
#define AFTER_HOURS_REPLACE_LOGGING
#include <cstdio>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

inline std::vector<std::string> &captured_warnings() {
  static std::vector<std::string> v;
  return v;
}

template <typename... Args>
inline void log_warn(std::format_string<Args...> fmt, Args &&...args) {
  captured_warnings().push_back(std::format(fmt, std::forward<Args>(args)...));
}
template <typename... Args>
inline void log_error(std::format_string<Args...> fmt, Args &&...args) {
  std::fprintf(stderr, "[ERROR] %s\n",
               std::format(fmt, std::forward<Args>(args)...).c_str());
}
template <typename... Args>
inline void log_trace(std::format_string<Args...>, Args &&...) {}
template <typename... Args>
inline void log_info(std::format_string<Args...>, Args &&...) {}
template <typename... Args>
inline void log_clean(std::format_string<Args...>, Args &&...) {}
template <typename Duration, typename... Args>
inline void log_once_per(Duration, int, std::format_string<Args...>,
                         Args &&...) {}

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
static bool expect_warn(std::string_view needle, const char *file, int line) {
  auto &w = captured_warnings();
  size_t before = w.size();
  std::erase_if(w, [&](const std::string &m) {
    return m.find(needle) != std::string::npos;
  });
  bool found = w.size() < before;
  check(found, "expected a layout warning", file, line);
  return found;
}

#define EXPECT_WARN(needle) expect_warn((needle), __FILE__, __LINE__)

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
struct TestLayout {
  std::vector<std::unique_ptr<Entity>> entities;
  window_manager::Resolution resolution{1280, 720};

  Entity &make_entity() {
    entities.push_back(std::make_unique<Entity>());
    return *entities.back();
  }
  Entity &make_ui(Size w, Size h) {
    Entity &e = make_entity();
    auto &ui = e.addComponent<UIComponent>(e.id);
    ui.set_desired_width(w);
    ui.set_desired_height(h);
    return e;
  }
  void add_child(Entity &parent, Entity &child) {
    auto &pc = parent.get<UIComponent>();
    auto &cc = child.get<UIComponent>();
    pc.add_child(child.id);
    cc.set_parent(parent.id);
  }
  float ui_scale = 1.0f;

  void run(Entity &root) { run_without(root, -1); }
  void run_without(Entity &root, EntityID omit) {
    EntityID max_id = 0;
    for (auto &e : entities) {
      max_id = std::max(max_id, e->id);
    }
    std::vector<Entity *> mapping(static_cast<size_t>(max_id) + 1, nullptr);
    for (auto &e : entities) {
      if (e->id == omit)
        continue;
      mapping[e->id] = e.get();
    }
    AutoLayout::autolayout(root.get<UIComponent>(), resolution, mapping,
                           /*enable_grid_snapping=*/false, ui_scale);
  }
  Entity &make_ui_adaptive(Size w, Size h) {
    Entity &e = make_ui(w, h);
    e.get<UIComponent>().resolved_scaling_mode = ScalingMode::Adaptive;
    return e;
  }
  static void set_adaptive(Entity &e) {
    e.get<UIComponent>().resolved_scaling_mode = ScalingMode::Adaptive;
  }
  static UIComponent &ui(Entity &e) { return e.get<UIComponent>(); }
};
static void set_abs(Entity &e, AbsoluteAnchor anchor, Size ox, Size oy) {
  auto &u = TestLayout::ui(e);
  u.make_absolute();
  u.absolute_anchor = anchor;
  u.absolute_offset_x = ox; u.absolute_offset_y = oy;
  u.absolute_offset_is_size = true;
}

TEST(trailing_pixels_exact) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(300));
  auto &child = t.make_ui(pixels(80), pixels(40));
  set_abs(child, AbsoluteAnchor::TrailingBottom, pixels(18), pixels(6));
  t.add_child(root, child); t.run(root);
  auto r = t.ui(child).rect();
  CHECK_APPROX(r.x, 302.f); CHECK_APPROX(r.y, 254.f);
  CHECK_APPROX(r.width, 80.f); CHECK_APPROX(r.height, 40.f);
}

TEST(trailing_percent_offset_and_size) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(300));
  auto &child = t.make_ui(percent(0.25f), percent(0.2f));
  set_abs(child, AbsoluteAnchor::TrailingTop, percent(0.05f), percent(0.1f));
  t.add_child(root, child); t.run(root);
  auto r = t.ui(child).rect();
  CHECK_APPROX(r.width, 100.f); CHECK_APPROX(r.height, 60.f);
  CHECK_APPROX(r.x, 280.f);
  CHECK_APPROX(r.y, 30.f);
}

TEST(leading_percent_position) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(300));
  auto &child = t.make_ui(pixels(100), pixels(60));
  set_abs(child, AbsoluteAnchor::LeadingTop, percent(0.5f), percent(0.5f));
  t.add_child(root, child); t.run(root);
  auto r = t.ui(child).rect();
  CHECK_APPROX(r.x, 200.f); CHECK_APPROX(r.y, 150.f);
}

TEST(trailing_percent_across_parent_sizes) {
  struct Case { float pw, ph, cw, ch, ox, oy, wx, wy; };
  const Case cases[] = {
      {200.f, 100.f, 50.f, 20.f, 0.10f, 0.10f, 130.f, 70.f},
      {640.f, 480.f, 80.f, 40.f, 0.25f, 0.05f, 400.f, 416.f},
      {333.f, 222.f, 33.f, 22.f, 0.f, 0.f, 300.f, 200.f},
  };
  for (const auto &c : cases) {
    TestLayout t;
    auto &root = t.make_ui(pixels(c.pw), pixels(c.ph));
    auto &child = t.make_ui(pixels(c.cw), pixels(c.ch));
    set_abs(child, AbsoluteAnchor::TrailingBottom, percent(c.ox), percent(c.oy));
    t.add_child(root, child); t.run(root);
    auto r = t.ui(child).rect();
    CHECK_APPROX(r.x, c.wx); CHECK_APPROX(r.y, c.wy);
  }
}

TEST(trailing_anchor_uses_content_box) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(300));
  t.ui(root).set_desired_padding(pixels(20), Axis::X);
  t.ui(root).set_desired_padding(pixels(10), Axis::Y);
  auto &child = t.make_ui(pixels(80), pixels(40));
  set_abs(child, AbsoluteAnchor::TrailingBottom, pixels(0), pixels(0));
  t.add_child(root, child); t.run(root);
  auto r = t.ui(child).rect();
  CHECK_APPROX(r.x, 300.f); CHECK_APPROX(r.y, 250.f);
  CHECK_APPROX(r.x + r.width, 380.f);
}

TEST(leading_pixels_unchanged) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(300));
  auto &child = t.make_ui(percent(0.5f), percent(0.5f));
  auto &u = t.ui(child); u.make_absolute();
  u.absolute_pos_x = 18.f; u.absolute_pos_y = 6.f;
  t.add_child(root, child); t.run(root);
  auto r = t.ui(child).rect();
  CHECK_APPROX(r.x, 18.f); CHECK_APPROX(r.y, 6.f);
  CHECK_APPROX(r.width, 200.f); CHECK_APPROX(r.height, 150.f);
}

TEST(trailing_via_absolute_pos_fields) {
  TestLayout t;
  auto &root = t.make_ui(pixels(400), pixels(300));
  auto &child = t.make_ui(pixels(80), pixels(40));
  auto &u = t.ui(child); u.make_absolute();
  u.absolute_anchor = AbsoluteAnchor::TrailingTop;
  u.absolute_pos_x = 18.f; u.absolute_pos_y = 6.f;
  t.add_child(root, child); t.run(root);
  auto r = t.ui(child).rect();
  CHECK_APPROX(r.x, 302.f); CHECK_APPROX(r.y, 6.f);
}

int main() {
  printf("=== Trailing Anchor Tests ===\n\n");
  int unexpected = 0;
  for (auto &[name, fn] : test_registry()) {
    printf("  Running: %s\n", name);
    captured_warnings().clear(); fn();
    for (const std::string &w : captured_warnings()) { fprintf(stderr, "  UNEXPECTED WARNING in %s: %s\n", name, w.c_str()); unexpected++; }
  }
  printf("\n%d/%d checks passed\n", tests_passed, tests_run);
  if (unexpected > 0) { printf("UNDECLARED WARNINGS: %d\n", unexpected); return 1; }
  return tests_passed == tests_run ? 0 : 1;
}
