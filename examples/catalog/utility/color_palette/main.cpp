#include <cstdio>

#include "../../../../src/plugins/color.h"

using namespace afterhours;

int main() {
  constexpr Color sky = colors::SKYBLUE;
  constexpr Color card = colors::RAYWHITE;
  constexpr Color alert = colors::UI_RED;

  std::printf("SKYBLUE  %d,%d,%d\n", sky.r, sky.g, sky.b);
  std::printf("RAYWHITE %d,%d,%d\n", card.r, card.g, card.b);
  std::printf("UI_RED   %d,%d,%d\n", alert.r, alert.g, alert.b);

  static_assert(sky.r == 102 && sky.g == 191 && sky.b == 255);
  static_assert(alert.r == colors::RED.r && alert.g == colors::RED.g);
  return 0;
}
