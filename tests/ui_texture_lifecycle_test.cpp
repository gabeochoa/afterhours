#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

TEST(reused_widget_releases_only_its_configured_texture_reference) {
  ImmTestHarness h;
  TextureType shared{64, 48};
  TextureType replacement{32, 16};
  auto config = ComponentConfig{}.with_size({pixels(100), pixels(60)});
  auto build = [&](ComponentConfig style) {
    return div(h.context(), mk(h.root(), 0), style);
  };
  auto image = build(config.with_texture(shared));
  auto other = div(h.context(), mk(h.root(), 1), config);
  const auto id = image.id();
  CHECK(image.ent().has<texture_manager::HasTexture>());
  CHECK(other.ent().has<texture_manager::HasTexture>());
  h.begin_frame();
  auto text = build(ComponentConfig{}
      .with_size({pixels(100), pixels(60)}).with_label("Image removed"));
  CHECK(text.id() == id);
  CHECK(!text.ent().has<texture_manager::HasTexture>());
  CHECK(other.ent().has<texture_manager::HasTexture>());
  CHECK(other.ent().get<texture_manager::HasTexture>().texture.width == 64);
  CHECK(shared.width == 64);
  h.begin_frame();
  auto restored = build(config.with_texture(replacement));
  CHECK(restored.id() == id);
  CHECK(restored.ent().has<texture_manager::HasTexture>());
  CHECK(restored.ent().get<texture_manager::HasTexture>().texture.width == 32);
}

TEST(manually_attached_texture_survives_unrelated_widget_configuration) {
  ImmTestHarness h;
  auto config = ComponentConfig{}.with_size({pixels(100), pixels(60)});
  auto build = [&](ComponentConfig style) {
    return div(h.context(), mk(h.root(), 0), style);
  };
  auto image = build(config);
  image.ent().addComponent<texture_manager::HasTexture>(
      TextureType{64, 48}, texture_manager::HasTexture::Alignment::Center);
  h.begin_frame();
  auto rebuilt = build(config.with_label("Manual image"));
  CHECK(rebuilt.id() == image.id());
  CHECK(image.ent().has<texture_manager::HasTexture>());
  CHECK(image.ent().get<texture_manager::HasTexture>().texture.width == 64);
}

int main() { return ui_test::run_registered_tests("ui_texture_lifecycle"); }
