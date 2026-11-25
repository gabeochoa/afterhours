#include "ui_test_harness.h"
#include <afterhours/src/graphics.h>

#include <cstdio>
#include <filesystem>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

TEST(with_glow_sets_has_glow) {
  ui_test::ImmTestHarness h;
  h.begin_frame();
  auto el = div(h.context(), mk(h.root(), 0),
                ComponentConfig{}
                    .with_size({pixels(40), pixels(40)})
                    .with_glow(Color{80, 200, 255, 160}, 18.f));
  CHECK(el.ent().has<HasGlow>());
  if (el.ent().has<HasGlow>()) {
    CHECK(el.ent().get<HasGlow>().glow.radius == 18.f);
    CHECK(el.ent().get<HasGlow>().glow.color.g == 200);
  }
  auto other = div(h.context(), mk(h.root(), 1),
                   ComponentConfig{}.with_size({pixels(40), pixels(40)}));
  CHECK(!other.ent().has<HasGlow>());
}

TEST(with_glow_glow_struct_overload) {
  ui_test::ImmTestHarness h;
  h.begin_frame();
  auto el = div(h.context(), mk(h.root(), 0),
                ComponentConfig{}
                    .with_size({pixels(40), pixels(40)})
                    .with_glow(Glow{22.f, Color{10, 20, 30, 40}}));
  CHECK(el.ent().has<HasGlow>());
  if (el.ent().has<HasGlow>()) {
    CHECK(el.ent().get<HasGlow>().glow.radius == 22.f);
    CHECK(el.ent().get<HasGlow>().glow.color.r == 10);
  }
}

TEST(apply_overrides_forwards_glow) {
  ComponentConfig base;
  ComponentConfig over;
  over.with_glow(Color{80, 200, 255, 160}, 18.f);
  CHECK(over.has_glow());
  base.apply_overrides_from(over);
  CHECK(base.has_glow());
  if (base.has_glow()) {
    CHECK(base.glow_config->radius == 18.f);
    CHECK(base.glow_config->color.b == 255);
  }
  ComponentConfig plain;
  plain.apply_overrides_from(ComponentConfig{});
  CHECK(!plain.has_glow());
}

TEST(removing_glow_toggles_has_glow_off) {
  ui_test::ImmTestHarness h;
  auto build = [&](bool with) {
    ComponentConfig cfg =
        ComponentConfig{}.with_size({pixels(40), pixels(40)});
    if (with)
      cfg.with_glow(Color{80, 200, 255, 160}, 18.f);
    return div(h.context(), mk(h.root(), 0), cfg);
  };
  h.begin_frame();
  auto first = build(true);
  const EntityID id = first.ent().id;
  CHECK(first.ent().has<HasGlow>());
  h.begin_frame();
  auto second = build(false);
  CHECK(second.ent().id == id);
  CHECK(!second.ent().has<HasGlow>());
}

static void check_glow_pixels(bool batched) {
  ui_test::ImmTestHarness h;
  FontManager fonts;
  fonts.load_font(UIComponent::DEFAULT_FONT, get_default_font());
  fonts.load_font(UIComponent::UNSET_FONT, get_default_font());
  h.begin_frame();
  auto el = div(h.context(), mk(h.root(), 0),
                ComponentConfig{}
                    .with_size({pixels(100), pixels(60)})
                    .with_absolute_position(350, 270)
                    .with_skip_grid_snap(true)
                    .with_custom_background(Color{20, 30, 40, 255})
                    .with_roundness(0.5f)
                    .with_rounded_corners(std::bitset<4>(15))
                    .with_glow(Color{80, 200, 255, 160}, 18.f));
  h.layout_only();
  const auto rect = el.ent().get<UIComponent>().rect();
  graphics::begin_frame();
  graphics::clear_background(Color{0, 0, 0, 255});
  if (batched)
    RenderBatched<ui_test::TestInputAction>{}.for_each_with_derived(
        h.root(), h.context(), fonts, 0.f);
  else
    RenderImm<ui_test::TestInputAction>{}.for_each_with_derived(
        h.root(), h.context(), fonts, 0.f);
  draw_radial_glow(Vector2Type{620.f, 140.f}, 42.f, 42.f,
                   Color{80, 200, 255, 180});
  draw_radial_glow(Vector2Type{620.f, 140.f}, 0.f, 42.f,
                   Color{255, 0, 0, 255});
  draw_radial_glow(Vector2Type{620.f, 140.f}, 42.f, 42.f,
                   Color{255, 0, 0, 0});
  graphics::end_frame();
  auto capture = capture_render_texture_rgba(graphics::get_render_texture());
  CHECK(capture.has_value());
  if (!capture)
    return;
  const auto px = [&](int x, int y) {
    const size_t o = (static_cast<size_t>(y) * capture->width + x) * 4;
    return std::array<int, 3>{capture->pixels[o], capture->pixels[o + 1],
                              capture->pixels[o + 2]};
  };
  const int cx = static_cast<int>(rect.x + rect.width / 2.f);
  const auto near_edge = px(cx, static_cast<int>(rect.y) - 3);
  const auto mid_glow = px(cx, static_cast<int>(rect.y) - 11);
  const auto past_glow = px(cx, static_cast<int>(rect.y) - 26);
  CHECK(near_edge[1] > 20 && near_edge[2] > near_edge[0]);
  CHECK(mid_glow[1] > 0 && mid_glow[1] < near_edge[1]);
  CHECK(past_glow[0] == 0 && past_glow[1] == 0 && past_glow[2] == 0);
  const auto spot_center = px(620, 140);
  const auto spot_mid = px(641, 140);
  const auto spot_past = px(620 + 58, 140);
  CHECK(spot_center[1] > 40 && spot_center[2] > spot_center[0]);
  CHECK(spot_mid[1] > 0 && spot_mid[1] < spot_center[1]);
  CHECK(spot_past[0] == 0 && spot_past[1] == 0 && spot_past[2] == 0);
}

TEST(immediate_glow_pixels) { check_glow_pixels(false); }
TEST(batched_glow_pixels) { check_glow_pixels(true); }

TEST(showcase_png) {
  ui_test::ImmTestHarness h;
  FontManager fonts;
  fonts.load_font(UIComponent::DEFAULT_FONT, get_default_font());
  fonts.load_font(UIComponent::UNSET_FONT, get_default_font());
  h.begin_frame();
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size({pixels(360), pixels(200)})
          .with_absolute_position(90, 80)
          .with_skip_grid_snap(true)
          .with_custom_background(Color{16, 26, 38, 255})
          .with_roundness(0.35f)
          .with_rounded_corners(std::bitset<4>(15))
          .with_glow(Color{80, 200, 255, 170}, 26.f));
  div(h.context(), mk(h.root(), 1),
      ComponentConfig{}
          .with_size({pixels(300), pixels(34)})
          .with_absolute_position(120, 360)
          .with_skip_grid_snap(true)
          .with_custom_background(Color{24, 60, 52, 255})
          .with_roundness(0.5f)
          .with_rounded_corners(std::bitset<4>(15))
          .with_glow(Color{90, 255, 190, 160}, 18.f));
  h.layout_only();
  graphics::begin_frame();
  graphics::clear_background(Color{6, 8, 14, 255});
  RenderImm<ui_test::TestInputAction>{}.for_each_with_derived(
      h.root(), h.context(), fonts, 0.f);
  draw_radial_glow(Vector2Type{640.f, 170.f}, 78.f, 78.f,
                   Color{255, 214, 140, 190}, 14);
  graphics::end_frame();
  auto capture = capture_render_texture_rgba(graphics::get_render_texture());
  CHECK(capture.has_value());
  if (!capture)
    return;
  raylib::Image img{const_cast<uint8_t *>(capture->pixels.data()),
                    capture->width, capture->height, 1,
                    raylib::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
  const char *path = "/tmp/pr-prim-glow.png";
  CHECK(raylib::ExportImage(img, path));
  CHECK(std::filesystem::exists(path));
  img.data = nullptr;
}

int main() {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 800;
  config.height = 600;
  if (!graphics::init(config))
    return 1;
  const int result = ui_test::run_registered_tests("ui glow");
  graphics::shutdown();
  return result;
}
