// origin_rounding_test.cpp
// Widget origins must rasterize on device pixels: the rect the renderer
// paints (recorded by the none backend) must have an origin whose
// effective-scale multiple is integral, at ui_scale 1 / 1.5 / 2 and under
// fractional parents. Sizes must be untouched. Layout rects stay fractional
// on purpose -- only the paint snaps.
#include "ui_test_harness.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

static bool on_grid(float v, float scale) {
  const float dev = v * scale;
  return std::fabs(dev - std::round(dev)) < 1e-3f;
}

static ComponentConfig box(float w, float h) {
  return ComponentConfig{}
      .with_size({pixels(w), pixels(h)})
      .with_background(Theme::Usage::Primary)
      .with_corner_radius(0);
}

// Every painted background rect of this frame must start on the grid;
// returns the rects so a case can also pin sizes.
static std::vector<RectangleType> painted_boxes(ImmTestHarness &h,
                                                float scale) {
  h.render();
  std::vector<RectangleType> out;
  for (const auto &c : h.drawn("rectangle_rounded")) {
    printf("  painted origin=(%.3f, %.3f) size=(%.3f, %.3f) dev=(%.3f,"
           " %.3f)\n",
           c.rect.x, c.rect.y, c.rect.width, c.rect.height,
           c.rect.x * scale, c.rect.y * scale);
    CHECK(on_grid(c.rect.x, scale));
    CHECK(on_grid(c.rect.y, scale));
    out.push_back(c.rect);
  }
  return out;
}

// Centred child in an odd-size parent: (401-100)/2 = 150.5 layout origin.
static void centered_case(float scale, bool adaptive) {
  ImmTestHarness h;
  h.layout_scale = scale;
  h.context().theme.ui_scale = scale;
  if (adaptive)
    h.context().scaling_mode = ScalingMode::Adaptive;
  auto parent = div(h.context(), mk(h.root(), 0),
                    box(401, 201)
                        .with_flex_direction(FlexDirection::Column)
                        .with_align_items(AlignItems::Center)
                        .with_justify_content(JustifyContent::Center)
                        .with_scaling_mode(adaptive ? ScalingMode::Adaptive
                                                    : ScalingMode::Proportional));
  auto child = div(h.context(), mk(parent.ent(), 0),
                   box(100, 27).with_scaling_mode(
                       adaptive ? ScalingMode::Adaptive
                                : ScalingMode::Proportional));
  (void)child;
  const float eff = adaptive ? scale : 1.0f;
  printf("centered scale %.1f adaptive=%d\n", scale, (int)adaptive);
  const auto boxes = painted_boxes(h, eff);
  CHECK(boxes.size() >= 2);
  // Layout stays fractional; only the paint snapped.
  const auto layout = parent.ent().get<UIComponent>().rect();
  (void)layout;
  for (const auto &r : boxes) {
    if (std::fabs(r.width - 100.f * eff) < 1e-3f)
      CHECK(std::fabs(r.height - 27.f * eff) < 1e-3f);
  }
}

int main() {
  centered_case(1.0f, false);
  centered_case(1.0f, true);
  centered_case(1.5f, true);
  centered_case(2.0f, true);

  // Absolute half-pixel origin (hanabi #110's 6x6 light at 932.5).
  {
    ImmTestHarness h;
    auto dot = div(h.context(), mk(h.root(), 0),
                   box(6, 6).with_absolute_position(400.5f, 300.5f));
    (void)dot;
    printf("absolute 6x6 at (400.5, 300.5)\n");
    const auto boxes = painted_boxes(h, 1.0f);
    CHECK(!boxes.empty());
    for (const auto &r : boxes)
      if (std::fabs(r.width - 6.f) < 1e-3f) {
        CHECK(std::fabs(r.x - 401.f) < 1e-3f);
        CHECK(std::fabs(r.y - 301.f) < 1e-3f);
      }
  }

  // Fractional parent: parent centred in the 800-wide root starts at
  // (800-401)/2 = 199.5, so its child inherits a fractional origin.
  {
    ImmTestHarness h;
    h.root().get<UIComponent>().set_align_items(AlignItems::Center);
    auto parent = div(h.context(), mk(h.root(), 0),
                      box(401, 201)
                          .with_flex_direction(FlexDirection::Column)
                          .with_align_items(AlignItems::Center));
    auto child = div(h.context(), mk(parent.ent(), 0), box(100, 27));
    (void)child;
    printf("child of fractional parent\n");
    const auto boxes = painted_boxes(h, 1.0f);
    CHECK(boxes.size() >= 2);
    // Parent layout origin is 199.5; both paints must still be integral.
    for (const auto &r : boxes)
      if (std::fabs(r.width - 100.f) < 1e-3f)
        CHECK(std::fabs(r.x - 350.f) < 1e-3f);
  }

  printf("\n%d/%d checks passed\n", ui_test::tests_passed(),
         ui_test::tests_run());
  return ui_test::tests_passed() == ui_test::tests_run() ? 0 : 1;
}
