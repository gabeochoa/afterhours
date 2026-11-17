#include <cstdio>
#include <type_traits>

namespace game {
struct Vec2 {
  float x, y;
};
struct Vec3 {
  float x, y, z;
};
struct Rect {
  float x, y, width, height;
};
struct Color {
  unsigned char r, g, b, a;
};
struct Font {};
struct Texture {
  float width, height;
};
} // namespace game

#define Vector2Type game::Vec2
#define Vector3Type game::Vec3
#define RectangleType game::Rect
#define ColorType game::Color
#define FontType game::Font
#define TextureType game::Texture

#include "../../../../ah.h"
#include "../../../../src/plugins/color.h"

static_assert(std::is_same_v<afterhours::Color, game::Color>);

int main() {
  game::Color brick{180, 60, 40, 255};
  game::Color lighter = afterhours::colors::lighten(brick, 0.25f);
  std::printf("brick %d,%d,%d lightened to %d,%d,%d\n", brick.r, brick.g,
              brick.b, lighter.r, lighter.g, lighter.b);
  return lighter.r > brick.r ? 0 : 1;
}
