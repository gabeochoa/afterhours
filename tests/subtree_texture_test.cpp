#include "ui_test_harness.h"
#include <afterhours/src/graphics.h>
using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
TEST(subtree_renders_to_texture_without_sibling_or_main_leak) {
  ui_test::ImmTestHarness h;
  FontManager fonts;
  fonts.load_font(UIComponent::DEFAULT_FONT, get_default_font());
  fonts.load_font(UIComponent::UNSET_FONT, get_default_font());
  h.begin_frame();
  auto subtree = div(h.context(), mk(h.root(), 0),
                     ComponentConfig{}
                         .with_size({pixels(40), pixels(40)})
                         .with_absolute_position(100, 100)
                         .with_custom_background(Color{220, 0, 0, 255})
                         .with_skip_grid_snap(true));
  div(h.context(), mk(subtree.ent(), 0),
      ComponentConfig{}
          .with_size({pixels(20), pixels(20)})
          .with_custom_background(Color{0, 200, 0, 255})
          .with_skip_grid_snap(true));
  div(h.context(), mk(h.root(), 1),
      ComponentConfig{}
          .with_size({pixels(40), pixels(40)})
          .with_absolute_position(300, 100)
          .with_custom_background(Color{0, 0, 220, 255})
          .with_skip_grid_snap(true));
  h.layout_only();
  auto rt = load_render_texture(40, 40);
  RenderImm<ui_test::TestInputAction> renderer;
  render_subtree_to_texture(renderer, h.context(), fonts, subtree.ent(), rt);
  auto img = capture_render_texture_rgba(rt);
  CHECK(img.has_value());
  if (img) {
    auto px = [&](int x, int y) {
      const auto i = static_cast<size_t>(y * img->width + x) * 4;
      return std::array<int, 3>{img->pixels[i], img->pixels[i + 1], img->pixels[i + 2]};
    };
    CHECK((px(10, 10) == std::array<int, 3>{0, 200, 0}));
    CHECK((px(30, 30) == std::array<int, 3>{220, 0, 0}));
    bool any_blue = false;
    for (size_t i = 0; i + 2 < img->pixels.size(); i += 4)
      any_blue |= img->pixels[i + 2] > 150 && img->pixels[i] < 80;
    CHECK(!any_blue);
  }
  unload_render_texture(rt);
  graphics::begin_frame();
  graphics::clear_background(Color{0, 0, 0, 255});
  graphics::end_frame();
  auto main_img = capture_render_texture_rgba(graphics::get_render_texture());
  CHECK(main_img.has_value());
  if (main_img) {
    const auto i = static_cast<size_t>(110 * main_img->width + 110) * 4;
    CHECK(main_img->pixels[i] == 0 && main_img->pixels[i + 1] == 0);
  }
}

int main() {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 800;
  config.height = 600;
  if (!graphics::init(config))
    return 1;
  const int result = ui_test::run_registered_tests("subtree texture");
  graphics::shutdown();
  return result;
}
