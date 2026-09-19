#include "ui_test_harness.h"
#include <afterhours/src/graphics.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

static void check_border_pixels(bool batched) {
  ui_test::ImmTestHarness h;
  FontManager fonts;
  fonts.load_font(UIComponent::DEFAULT_FONT, get_default_font());
  fonts.load_font(UIComponent::UNSET_FONT, get_default_font());
  const std::array<float, 6> widths{1, 3, 6, 1.5f, 3, 30};
  for (float zoom : {1.f, 2.f}) {
    h.begin_frame();
    h.context().theme.ui_scale = zoom;
    std::array<EntityID, 6> ids{};
    for (size_t i = 0; i < widths.size(); ++i) {
      auto style = ComponentConfig{}
          .with_size({pixels(40), pixels(40)})
          .with_absolute_position(20 + static_cast<float>(i) * 65, 20)
          .with_scaling_mode(ScalingMode::Adaptive).with_skip_grid_snap(true)
          .with_transparent_bg().with_border(Color{200, 100, 50, 128},
                                             i == 4 ? h720(3.6f) : pixels(widths[i]))
          .with_roundness(i == 0 || i == 5 ? 0.f : .5f)
          .with_rounded_corners(std::bitset<4>(i == 2 ? 5 : 15));
      ids[i] = div(h.context(), mk(h.root(), static_cast<int>(i)), style).ent().id;
    }
    h.layout_only();
    graphics::begin_frame();
    graphics::clear_background(Color{0, 0, 0, 255});
    if (batched)
      RenderBatched<ui_test::TestInputAction>{}.for_each_with_derived(h.root(), h.context(), fonts, 0.f);
    else
      RenderImm<ui_test::TestInputAction>{}.for_each_with_derived(h.root(), h.context(), fonts, 0.f);
    graphics::end_frame();
    auto capture = capture_render_texture_rgba(graphics::get_render_texture());
    CHECK(capture.has_value());
    if (!capture) continue;
    const auto red = [&](int x, int y) {
      return capture->pixels[(static_cast<size_t>(y) * capture->width + x) * 4];
    };
    for (size_t i = 0; i < ids.size(); ++i) {
      const auto rect = UICollectionHolder::getEntityForID(ids[i]).asE().get<UIComponent>().rect();
      const int x = static_cast<int>(rect.x), y = static_cast<int>(rect.y);
      const int w = static_cast<int>(rect.width), height = static_cast<int>(rect.height);
      const float thickness = std::min(i == 4 ? 3.f : widths[i] * zoom,
                                       std::min(rect.width, rect.height) * .5f);
      for (int offset = -1; offset <= height; ++offset) {
        const bool outside = offset < 0 || offset >= height;
        const bool edge = offset < std::floor(thickness) || offset >= height - std::floor(thickness);
        const bool inner = offset >= std::ceil(thickness) && offset < height - std::ceil(thickness);
        if (outside || inner) CHECK(red(x + w / 2, y + offset) == 0);
        else if (edge) CHECK(std::abs(red(x + w / 2, y + offset) - 100) <= 2);
      }
      for (int offset = -1; offset <= w; ++offset) {
        const bool outside = offset < 0 || offset >= w;
        const bool edge = offset < std::floor(thickness) || offset >= w - std::floor(thickness);
        const bool inner = offset >= std::ceil(thickness) && offset < w - std::ceil(thickness);
        if (outside || inner) CHECK(red(x + offset, y + height / 2) == 0);
        else if (edge) CHECK(std::abs(red(x + offset, y + height / 2) - 100) <= 2);
      }
      CHECK((red(x, y) > 0) == (i == 0 || i == 5));
      CHECK((red(x + w - 1, y) > 0) == (i == 0 || i == 2 || i == 5));
    }
  }
}

TEST(immediate_border_width_pixels) { check_border_pixels(false); }
TEST(batched_border_width_pixels) { check_border_pixels(true); }

TEST(border_follows_filled_corners) {
  for (bool batched : {false, true}) {
    ui_test::ImmTestHarness h;
    FontManager fonts;
    fonts.load_font(UIComponent::DEFAULT_FONT, get_default_font());
    fonts.load_font(UIComponent::UNSET_FONT, get_default_font());
    h.begin_frame();
    for (int corner = 0; corner < 4; ++corner) {
      for (int bordered = 0; bordered < 2; ++bordered) {
        auto style = ComponentConfig{}.with_size({pixels(40), pixels(40)})
            .with_absolute_position(20.f + corner * 140.f + bordered * 60.f, 20)
            .with_roundness(.5f).with_rounded_corners(std::bitset<4>(1 << corner))
            .with_custom_background(Color{0, 80, 160, 255});
        if (bordered) style.with_border(Color{200, 100, 50, 255}, pixels(3));
        div(h.context(), mk(h.root(), corner * 2 + bordered), style);
      }
    }
    h.layout_only();
    graphics::begin_frame();
    graphics::clear_background(Color{0, 0, 0, 255});
    if (batched)
      RenderBatched<ui_test::TestInputAction>{}.for_each_with_derived(h.root(), h.context(), fonts, 0.f);
    else
      RenderImm<ui_test::TestInputAction>{}.for_each_with_derived(h.root(), h.context(), fonts, 0.f);
    graphics::end_frame();
    auto capture = capture_render_texture_rgba(graphics::get_render_texture());
    CHECK(capture.has_value());
    if (!capture) continue;
    const auto painted = [&](int x, int y) {
      const size_t offset = (static_cast<size_t>(y) * capture->width + x) * 4;
      return capture->pixels[offset] || capture->pixels[offset + 1] || capture->pixels[offset + 2];
    };
    for (int corner = 0; corner < 4; ++corner)
      for (int x : {0, 39})
        for (int y : {0, 39})
          CHECK(painted(20 + corner * 140 + x, 20 + y) ==
                painted(80 + corner * 140 + x, 20 + y));
  }
}

int main() {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 800;
  config.height = 600;
  if (!graphics::init(config)) return 1;
  const int result = ui_test::run_registered_tests("border widths");
  graphics::shutdown();
  return result;
}
