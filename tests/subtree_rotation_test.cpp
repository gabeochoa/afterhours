#include "ui_test_harness.h"
#include <afterhours/src/graphics.h>

#include <cmath>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

namespace {
constexpr float PI_F = 3.14159265358979323846f;

Vector2Type rotate_manual(Vector2Type p, Vector2Type pivot, float deg) {
  const float rad = deg * PI_F / 180.f;
  const float c = std::cos(rad), s = std::sin(rad);
  const float dx = p.x - pivot.x, dy = p.y - pivot.y;
  return {pivot.x + dx * c - dy * s, pivot.y + dx * s + dy * c};
}

struct Scene {
  EntityID parent_id = -1;
  EntityID child_id = -1;
  EntityID sprite_id = -1;
};

Scene build_scene(ui_test::ImmTestHarness &h, float deg,
                  raylib::Texture2D texture) {
  h.begin_frame();
  auto parent = div(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size({pixels(200), pixels(100)})
                        .with_absolute_position(100, 100)
                        .with_rotation(deg)
                        .with_custom_background(Color{40, 40, 120, 255})
                        .with_debug_name("rot_parent"));
  auto child = button(h.context(), mk(parent.ent(), 0),
                      ComponentConfig{}
                          .with_size({pixels(60), pixels(30)})
                          .with_absolute_position(10, 10)
                          .with_custom_background(Color{255, 0, 0, 255})
                          .with_debug_name("rot_child"));
  auto sprite = image(h.context(), mk(parent.ent(), 1),
                      ComponentConfig{}
                          .with_size({pixels(30), pixels(30)})
                          .with_absolute_position(120, 10)
                          .with_texture(texture,
                                        texture_manager::HasTexture::Alignment::Center)
                          .with_debug_name("rot_sprite"));
  h.layout_only();
  return {parent.ent().id, child.ent().id, sprite.ent().id};
}

Entity &entity_of(EntityID id) {
  return UICollectionHolder::getEntityForID(id).asE();
}

Color pixel_at(const RGBACapture &cap, int x, int y) {
  const size_t off = (static_cast<size_t>(y) * static_cast<size_t>(cap.width) +
                      static_cast<size_t>(x)) * 4;
  return Color{cap.pixels[off], cap.pixels[off + 1], cap.pixels[off + 2],
               cap.pixels[off + 3]};
}

bool near_color(Color a, Color b, int tol = 12) {
  return std::abs(a.r - b.r) <= tol && std::abs(a.g - b.g) <= tol &&
         std::abs(a.b - b.b) <= tol;
}

void check_hit(float deg) {
  ui_test::ImmTestHarness h;
  auto img = raylib::GenImageColor(16, 16, Color{0, 255, 0, 255});
  auto tex = raylib::LoadTextureFromImage(img);
  raylib::UnloadImage(img);
  Scene s = build_scene(h, deg, tex);
  Entity &child = entity_of(s.child_id);
  const auto &cmp = child.get<UIComponent>();
  const RectangleType r = cmp.rect();
  const Vector2Type centre{r.x + r.width * .5f, r.y + r.height * .5f};
  const Vector2Type pivot{200.f, 150.f};
  const Vector2Type world = rotate_manual(centre, pivot, deg);
  const input::MousePosition at_world{world.x, world.y};
  const input::MousePosition at_flat{centre.x, centre.y};
  CHECK(afterhours::ui::detail::point_in_hit_rect(child, cmp, at_world));
  CHECK(!afterhours::ui::detail::point_in_hit_rect(child, cmp, at_flat));
  raylib::UnloadTexture(tex);
}

void check_render(bool batched, float deg) {
  ui_test::ImmTestHarness h;
  FontManager fonts;
  fonts.load_font(UIComponent::DEFAULT_FONT, get_default_font());
  fonts.load_font(UIComponent::UNSET_FONT, get_default_font());
  auto img = raylib::GenImageColor(16, 16, Color{0, 255, 0, 255});
  auto tex = raylib::LoadTextureFromImage(img);
  raylib::UnloadImage(img);
  build_scene(h, deg, tex);
  graphics::begin_frame();
  graphics::clear_background(Color{0, 0, 0, 255});
  if (batched)
    RenderBatched<ui_test::TestInputAction>{}.for_each_with_derived(
        h.root(), h.context(), fonts, 0.f);
  else
    RenderImm<ui_test::TestInputAction>{}.for_each_with_derived(
        h.root(), h.context(), fonts, 0.f);
  graphics::end_frame();
  auto capture = capture_render_texture_rgba(graphics::get_render_texture());
  CHECK(capture.has_value());
  if (!capture) {
    raylib::UnloadTexture(tex);
    return;
  }
  const Vector2Type pivot{200.f, 150.f};
  const Vector2Type child_flat{140.f, 125.f};
  const Vector2Type sprite_flat{235.f, 125.f};
  const Vector2Type child_world = rotate_manual(child_flat, pivot, deg);
  const Vector2Type sprite_world = rotate_manual(sprite_flat, pivot, deg);
  const Color child_px = pixel_at(*capture, static_cast<int>(child_world.x),
                                  static_cast<int>(child_world.y));
  const Color sprite_px = pixel_at(*capture, static_cast<int>(sprite_world.x),
                                   static_cast<int>(sprite_world.y));
  const Color flat_px = pixel_at(*capture, static_cast<int>(child_flat.x),
                                 static_cast<int>(child_flat.y));
  CHECK(near_color(child_px, Color{255, 0, 0, 255}));
  CHECK(near_color(sprite_px, Color{0, 255, 0, 255}));
  CHECK(!near_color(flat_px, Color{255, 0, 0, 255}));
  raylib::UnloadTexture(tex);
}
} // namespace

TEST(hit_child_inside_90_degree_parent) { check_hit(90.f); }
TEST(hit_child_inside_30_degree_parent) { check_hit(30.f); }
TEST(imm_render_rotated_subtree_90) { check_render(false, 90.f); }
TEST(batched_render_rotated_subtree_90) { check_render(true, 90.f); }
TEST(imm_render_rotated_subtree_30) { check_render(false, 30.f); }
TEST(batched_render_rotated_subtree_30) { check_render(true, 30.f); }

int main() {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 800;
  config.height = 600;
  if (!graphics::init(config))
    return 1;
  const int result = ui_test::run_registered_tests("subtree rotation");
  graphics::shutdown();
  return result;
}
