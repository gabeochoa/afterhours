#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;
using ui_test::TestInputAction;

static ComponentConfig positioned(float x, float y, float w, float h) {
  return ComponentConfig{}.with_size({pixels(w), pixels(h)})
      .with_absolute_position(x, y).with_corner_radius(0)
      .with_skip_grid_snap(true);
}

static std::vector<RenderPrimitive> collect(ImmTestHarness &h, Entity &ent,
                                            int layer = 0) {
  auto &fonts = *h.render_font();
  Arena arena(Arena::DEFAULT_CAPACITY);
  RenderCommandBuffer buffer(arena);
  RenderBatched<TestInputAction> renderer;
  renderer.collect(buffer, h.context(), fonts, ent, layer);
  const auto &cmds = buffer.commands();
  return std::vector<RenderPrimitive>(cmds.begin(), cmds.end());
}

static bool is_scissor(const RenderPrimitive &c, int x, int y, int w, int h) {
  return c.type == RenderPrimitiveType::ScissorStart &&
         c.data.scissor.x == x && c.data.scissor.y == y &&
         c.data.scissor.width == w && c.data.scissor.height == h;
}

TEST(nested_scroll_background_and_border_are_scissored_to_outer_viewport) {
  ImmTestHarness h;
  h.begin_frame();
  auto outer = div(h.context(), mk(h.root(), 0),
                   positioned(0, 0, 200, 100)
                       .with_overflow(Overflow::Scroll, Axis::Y)
                       .with_transparent_bg());
  const ColorType inner_bg{12, 34, 56, 255};
  auto inner = div(h.context(), mk(outer.ent(), 0),
                   positioned(0, 0, 200, 150)
                       .with_overflow(Overflow::Scroll, Axis::Y)
                       .with_custom_background(inner_bg)
                       .with_border(ColorType{200, 100, 50, 255}, pixels(3)));
  h.layout_only();
  const auto cmds = collect(h, inner.ent());
  int scissor_at = -1, fill_at = -1, end_at = -1;
  for (int i = 0; i < static_cast<int>(cmds.size()); i++) {
    if (cmds[i].entity_id != inner.id()) continue;
    if (scissor_at < 0 && is_scissor(cmds[i], 0, 0, 200, 100)) scissor_at = i;
    if (fill_at < 0 && cmds[i].data.rectangle.fill_color.r == inner_bg.r &&
        cmds[i].data.rectangle.fill_color.g == inner_bg.g &&
        (cmds[i].type == RenderPrimitiveType::RoundedRectangle ||
         cmds[i].type == RenderPrimitiveType::Rectangle)) {
      fill_at = i;
      CHECK_APPROX(cmds[i].data.rectangle.rect.height, 150.f);
    }
    if (fill_at >= 0 && cmds[i].type == RenderPrimitiveType::ScissorEnd)
      end_at = i;
  }
  CHECK(scissor_at >= 0);
  CHECK(fill_at > scissor_at);
  CHECK(end_at > fill_at);
}

TEST(top_level_scroll_view_is_not_scissored_to_itself) {
  ImmTestHarness h;
  h.begin_frame();
  auto view = div(h.context(), mk(h.root(), 0),
                  positioned(10, 10, 200, 150)
                      .with_overflow(Overflow::Scroll, Axis::Y)
                      .with_custom_background(ColorType{12, 34, 56, 255}));
  h.layout_only();
  const auto cmds = collect(h, view.ent());
  for (const auto &c : cmds) {
    if (c.entity_id != view.id()) continue;
    CHECK(c.type != RenderPrimitiveType::ScissorStart);
  }
}

int main() { return ui_test::run_registered_tests("scroll clip decoration"); }
