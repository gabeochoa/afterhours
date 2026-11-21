#include "ui_test_harness.h"
#include <afterhours/src/graphics.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

// Border-only element: HasBorder, no HasColor. The auto transparent HasColor
// from apply_config is removed after layout to model overlay/raw elements,
// which never get one. The border must still draw in both renderers.
static void check_border_only_pixels(bool batched) {
  ui_test::ImmTestHarness h;
  FontManager fonts;
  fonts.load_font(UIComponent::DEFAULT_FONT, get_default_font());
  fonts.load_font(UIComponent::UNSET_FONT, get_default_font());
  h.begin_frame();
  auto style = ComponentConfig{}
      .with_size({pixels(40), pixels(40)})
      .with_absolute_position(20, 20)
      .with_border(Color{200, 100, 50, 255}, pixels(3));
  EntityID id = div(h.context(), mk(h.root(), 0), style).ent().id;
  h.layout_only();
  auto &e = UICollectionHolder::getEntityForID(id).asE();
  CHECK(e.has<HasBorder>());
  e.removeComponentIfExists<HasColor>();
  CHECK(!e.has<HasColor>());
  graphics::begin_frame();
  graphics::clear_background(Color{0, 0, 0, 255});
  if (batched)
    RenderBatched<ui_test::TestInputAction>{}.for_each_with_derived(h.root(), h.context(), fonts, 0.f);
  else
    RenderImm<ui_test::TestInputAction>{}.for_each_with_derived(h.root(), h.context(), fonts, 0.f);
  graphics::end_frame();
  auto capture = capture_render_texture_rgba(graphics::get_render_texture());
  CHECK(capture.has_value());
  if (!capture) return;
  const auto red = [&](int x, int y) {
    return capture->pixels[(static_cast<size_t>(y) * capture->width + x) * 4];
  };
  const auto rect = e.get<UIComponent>().rect();
  const int x = static_cast<int>(rect.x), y = static_cast<int>(rect.y);
  // Top edge midpoint must carry the border red channel.
  CHECK(red(x + 20, y + 1) > 100);
  CHECK(red(x + 20, y + 20) == 0);  // no fill inside
}

TEST(immediate_border_only_without_has_color) { check_border_only_pixels(false); }
TEST(batched_border_only_without_has_color) { check_border_only_pixels(true); }

int main() {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 800;
  config.height = 600;
  if (!graphics::init(config)) return 1;
  const int result = ui_test::run_registered_tests("border only");
  graphics::shutdown();
  return result;
}
