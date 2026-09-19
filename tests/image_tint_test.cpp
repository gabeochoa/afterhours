#include "ui_test_harness.h"
#include <afterhours/src/graphics.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

static void check_rendering(bool batched) {
  const Color source_color{160, 200, 240, 255};
  auto bitmap = raylib::GenImageColor(4, 4, source_color);
  auto atlas_bitmap = raylib::GenImageColor(8, 4, raylib::MAGENTA);
  raylib::ImageDrawRectangle(&atlas_bitmap, 4, 0, 4, 4, source_color);
  const auto texture = raylib::LoadTextureFromImage(bitmap);
  const auto atlas = raylib::LoadTextureFromImage(atlas_bitmap);
  raylib::UnloadImage(bitmap);
  raylib::UnloadImage(atlas_bitmap);
  const std::array<Color, 3> tints{{{255, 64, 32, 255}, {32, 255, 64, 255}, {64, 32, 255, 255}}};
  ui_test::ImmTestHarness h;
  FontManager fonts;
  fonts.load_font(UIComponent::DEFAULT_FONT, get_default_font());
  fonts.load_font(UIComponent::UNSET_FONT, get_default_font());
  std::array<EntityID, 3> ids{};
  for (int phase = 0; phase < 3; ++phase) {
    h.begin_frame();
    for (int i = 0; i < 3; ++i) {
      auto parent = div(h.context(), mk(h.root(), i), ComponentConfig{}
          .with_size({pixels(32), pixels(32)})
          .with_absolute_position(10.f + static_cast<float>(i) * 50.f, 20)
          .with_transparent_bg().with_opacity(phase == 1 ? .5f : 1.f));
      auto style = ComponentConfig{}.with_size({pixels(32), pixels(32)})
          .with_transparent_bg().with_opacity(phase == 1 ? .5f : 1.f);
      auto tint = tints[(static_cast<size_t>(i) + (phase == 1 ? 1 : 0)) % tints.size()];
      if (phase == 1) tint.a = 128;
      if (phase != 2) style.with_image_tint(tint);
      Entity *item = nullptr;
      if (i == 0)
        item = &image(h.context(), mk(parent.ent(), 0), style.with_texture(
            texture, texture_manager::HasTexture::Alignment::Center)).ent();
      if (i == 1)
        item = &sprite(h.context(), mk(parent.ent(), 0), atlas, {4, 0, 4, 4}, style).ent();
      if (i == 2)
        item = &image_button(h.context(), mk(parent.ent(), 0), atlas, {4, 0, 4, 4}, style).ent();
      CHECK(item != nullptr);
      if (!item) continue;
      if (phase == 0) ids[static_cast<size_t>(i)] = item->id;
      CHECK(item->id == ids[static_cast<size_t>(i)]);
      CHECK(item->has<HasImageTint>() == (phase != 2));
    }
    h.layout_only();
    graphics::begin_frame();
    graphics::clear_background(Color{0, 0, 0, 255});
    if (batched)
      RenderBatched<ui_test::TestInputAction>{}.for_each_with_derived(h.root(), h.context(), fonts, 0.f);
    else
      RenderImm<ui_test::TestInputAction>{}.for_each_with_derived(h.root(), h.context(), fonts, 0.f);
    graphics::end_frame();
    const auto capture = capture_render_texture_rgba(graphics::get_render_texture());
    CHECK(capture.has_value());
    if (!capture) continue;
    for (size_t i = 0; i < 3; ++i) {
      const auto &cmp = UICollectionHolder::getEntityForID(ids[i]).asE().get<UIComponent>();
      const auto rect = cmp.rect();
      const size_t x = static_cast<size_t>(rect.x + rect.width / 2);
      const size_t y = static_cast<size_t>(rect.y + rect.height / 2);
      const size_t offset = (y * static_cast<size_t>(capture->width) + x) * 4;
      CHECK(offset + 3 < capture->pixels.size());
      if (offset + 3 >= capture->pixels.size()) continue;
      const auto tint = phase == 2 ? Color{255, 255, 255, 255} : tints[(i + (phase == 1 ? 1 : 0)) % tints.size()];
      const float alpha = phase == 1 ? 32.f / 255.f : 1.f;
      CHECK(std::abs(capture->pixels[offset] - source_color.r * tint.r / 255.f * alpha) <= 2.f);
      CHECK(std::abs(capture->pixels[offset + 1] - source_color.g * tint.g / 255.f * alpha) <= 2.f);
      CHECK(std::abs(capture->pixels[offset + 2] - source_color.b * tint.b / 255.f * alpha) <= 2.f);
    }
  }
  raylib::UnloadTexture(texture);
  raylib::UnloadTexture(atlas);
}

TEST(immediate_image_tint_pixels_and_reset) { check_rendering(false); }
TEST(batched_image_tint_pixels_and_reset) { check_rendering(true); }

TEST(image_tint_merges_and_visual_overlays_preserve_it) {
  const auto base = ComponentConfig{}.with_image_tint({10, 20, 30, 255});
  const auto kept = base.apply_overrides(ComponentConfig{});
  CHECK(kept.image_tint->r == 10);
  const auto replaced = base.apply_overrides(ComponentConfig{}.with_image_tint({255, 255, 255, 255}));
  CHECK(replaced.image_tint->r == 255);
  auto inherited = ComponentConfig{}.apply_inheritable_from(base);
  CHECK(inherited.image_tint.has_value());
  CHECK(inherited.image_tint->g == 20);
  ui_test::ImmTestHarness h;
  auto item = div(h.context(), mk(h.root(), 0), base);
  ui::imm::detail::apply_visuals(h.context(), item.ent(), ComponentConfig{}, true);
  CHECK(item.ent().get<HasImageTint>().color.g == 20);
  ui::imm::detail::apply_visuals(h.context(), item.ent(), ComponentConfig{}, false);
  CHECK(!item.ent().has<HasImageTint>());
}

int main() {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 180;
  config.height = 100;
  if (!graphics::init(config)) return 1;
  const int result = ui_test::run_registered_tests("image tint");
  graphics::shutdown();
  return result;
}
