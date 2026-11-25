#define FMT_HEADER_ONLY
#define AFTER_HOURS_USE_NONE
#include <fmt/format.h>
#define AFTER_HOURS_REPLACE_LOGGING
#include <cstdio>
#include <format>
#include <string>
#include <utility>
#include <vector>
template <typename... Args>
inline void log_warn(std::format_string<Args...>, Args &&...) {}
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
#include <afterhours/src/plugins/ui/context.h>
#include <afterhours/src/plugins/ui/component_config.h>
#include <cmath>
#include <memory>
using namespace afterhours;
using namespace afterhours::ui;
static int checks_run = 0;
static int checks_passed = 0;
static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s\n", what.c_str());
  }
}
static bool approx(float a, float b) { return std::fabs(a - b) < 0.5f; }
struct Fixture {
  std::vector<std::unique_ptr<Entity>> entities;
  Entity &scroll_ent;
  Entity &child_ent;
  explicit Fixture(bool gutter, bool vertical = true, bool horizontal = false)
      : scroll_ent(make()), child_ent(make()) {
    auto &sv = scroll_ent.addComponent<HasScrollView>();
    sv.vertical_enabled = vertical;
    sv.horizontal_enabled = horizontal;
    sv.scrollbar_gutter = gutter;
    sv.scrollbar_thickness = pixels(10.f);
    auto &sui = scroll_ent.addComponent<UIComponent>(scroll_ent.id);
    sui.set_desired_width(pixels(200.f));
    sui.set_desired_height(pixels(100.f));
    auto &cui = child_ent.addComponent<UIComponent>(child_ent.id);
    cui.set_desired_width(percent(1.0f));
    cui.set_desired_height(percent(1.0f));
    sui.add_child(child_ent.id);
    cui.set_parent(scroll_ent.id);
    run();
  }
  Entity &make() {
    entities.push_back(std::make_unique<Entity>());
    return *entities.back();
  }
  void run() {
    EntityID max_id = 0;
    for (auto &e : entities)
      max_id = std::max(max_id, e->id);
    std::vector<Entity *> mapping(static_cast<size_t>(max_id) + 1, nullptr);
    for (auto &e : entities)
      mapping[e->id] = e.get();
    AutoLayout::autolayout(scroll_ent.get<UIComponent>(),
                           window_manager::Resolution{1280, 720}, mapping,
                           /*enable_grid_snapping=*/false);
  }
  UIComponent &scroll_ui() { return scroll_ent.get<UIComponent>(); }
  UIComponent &child_ui() { return child_ent.get<UIComponent>(); }
};
int main() {
  printf("=== scrollbar gutter tests ===\n\n");
  check(!HasScrollView{}.scrollbar_gutter,
        "gutter is off by default, so no existing view moves");
  {
    Fixture f(false);
    check(approx(f.scroll_ui().computed_padd[Axis::right], 0.f),
          "default: no right padding is reserved");
    check(approx(f.child_ui().computed[Axis::X], 200.f),
          "default: child content width is the full view width");
  }
  {
    Fixture f(true);
    check(approx(f.scroll_ui().computed_padd[Axis::right], 10.f),
          "gutter: right padding grows by the bar thickness");
    check(approx(f.scroll_ui().computed_padd[Axis::X], 10.f),
          "gutter: total x padding is just the gutter (no user padding)");
    check(approx(f.child_ui().computed[Axis::X], 190.f),
          "gutter: child content width shrinks by the bar thickness");
    check(approx(f.scroll_ui().computed_padd[Axis::bottom], 0.f),
          "gutter: a vertical bar reserves nothing at the bottom");
    check(approx(f.child_ui().computed[Axis::Y], 100.f),
          "gutter: child height is untouched by a vertical bar");
  }
  {
    Fixture fits(true);
    const float width_when_fits = fits.child_ui().computed[Axis::X];
    fits.child_ui().set_desired_height(pixels(500.f));
    fits.run();
    check(approx(fits.child_ui().computed[Axis::X], width_when_fits),
          "gutter: content width is the same before and after overflow");
    check(approx(fits.scroll_ui().computed_padd[Axis::right], 10.f),
          "gutter: reservation survives a second layout pass (not doubled)");
  }
  {
    Fixture f(true, /*vertical=*/false, /*horizontal=*/true);
    check(approx(f.scroll_ui().computed_padd[Axis::bottom], 10.f),
          "gutter: a horizontal bar reserves the bottom");
    check(approx(f.child_ui().computed[Axis::Y], 90.f),
          "gutter: child height shrinks by the bar thickness");
    check(approx(f.child_ui().computed[Axis::X], 200.f),
          "gutter: child width is untouched by a horizontal bar");
  }
  {
    Fixture f(true, true, true);
    check(approx(f.child_ui().computed[Axis::X], 190.f) &&
              approx(f.child_ui().computed[Axis::Y], 90.f),
          "gutter: both axes reserve their trailing edge");
  }
  {
    Fixture f(true);
    f.scroll_ui().set_desired_padding(pixels(4.f), Axis::X);
    f.run();
    check(approx(f.scroll_ui().computed_padd[Axis::right], 14.f),
          "gutter: user right padding (4) plus gutter (10) is 14");
    check(approx(f.child_ui().computed[Axis::X], 200.f - 4.f - 14.f),
          "gutter: child width accounts for user padding and gutter");
  }
  {
    Fixture f(true);
    auto &sv = f.scroll_ent.get<HasScrollView>();
    sv.content_size = {190.f, 800.f};
    sv.viewport_size = {190.f, 100.f};
    const RectangleType view{0.f, 0.f, 200.f, 100.f};
    const auto g = scrollbar_geometry(sv, view, true, 10.f, 24.f);
    check(g.visible, "gutter: bar is visible when the content overflows");
    check(approx(g.track.x, f.child_ui().computed[Axis::X]),
          "gutter: track sits in the reserved space, past the content");
  }
  {
    imm::ComponentConfig cfg;
    check(!cfg.scrollbar_gutter, "config: gutter is off by default");
    cfg.with_scrollbar_gutter();
    check(cfg.scrollbar_gutter, "config: with_scrollbar_gutter opts in");
    imm::ComponentConfig merged = imm::ComponentConfig{}.apply_overrides(cfg);
    check(merged.scrollbar_gutter,
          "config: an overlay that opts in wins over the base config");
  }
  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
