#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using namespace ui_test;

static ComponentConfig positioned(float x, float y, float w, float height) {
  return ComponentConfig{}.with_size({pixels(w), pixels(height)})
      .with_absolute_position(x, y).with_corner_radius(0)
      .with_skip_grid_snap(true).with_transparent_bg();
}

TEST(context_callback_receives_scale_two_and_maps_a_point) {
  Arena arena(Arena::DEFAULT_CAPACITY);
  RenderCommandBuffer buffer(arena);
  BatchedRenderer renderer;
  FontManager fonts;
  std::vector<RenderPrimitive::DrawContext> seen;
  RenderPrimitive::CustomDrawContextFn draw =
      [&](const RenderPrimitive::DrawContext &d) { seen.push_back(d); };
  buffer.add_custom({10.f, 20.f, 40.f, 30.f}, &draw, 2.f, 0, 0);
  renderer.render(buffer, fonts);
  CHECK(seen.size() == 1);
  if (seen.empty()) return;
  CHECK_APPROX(seen[0].scale, 2.f);
  CHECK_APPROX(seen[0].rect.x + 4.f * seen[0].scale, 18.f);
  CHECK_APPROX(seen[0].rect.y + 4.f * seen[0].scale, 28.f);
}

TEST(adaptive_widget_gets_ui_scale_in_draw_context) {
  ImmTestHarness h;
  h.context().theme.ui_scale = 2.f;
  float seen_scale = 0.f;
  float seen_px = 0.f;
  auto box = div(h.context(), mk(h.root(), 0), positioned(10, 10, 100, 50)
      .with_scaling_mode(ScalingMode::Adaptive)
      .with_custom_background({10, 20, 30, 255})
      .with_on_draw_fg([&](const RenderPrimitive::DrawContext &d) {
        seen_scale = d.scale;
        seen_px = d.rect.x + 4.f * d.scale;
      }));
  (void)box;
  h.render();
  CHECK_APPROX(seen_scale, 2.f);
  CHECK(seen_px > 10.f);
}

TEST(proportional_widget_gets_scale_one_despite_ui_scale) {
  ImmTestHarness h;
  h.context().theme.ui_scale = 2.f;
  float seen_scale = 0.f;
  auto box = div(h.context(), mk(h.root(), 0), positioned(10, 10, 100, 50)
      .with_scaling_mode(ScalingMode::Proportional)
      .with_custom_background({10, 20, 30, 255})
      .with_on_draw_fg([&](const RenderPrimitive::DrawContext &d) {
        seen_scale = d.scale;
      }));
  (void)box;
  h.render_batched();
  CHECK_APPROX(seen_scale, 1.f);
}

int main() { return run_registered_tests("draw_context_scale"); }
