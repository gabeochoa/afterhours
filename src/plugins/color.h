
#pragma once

#include "../base_component.h"
#include "../developer.h"

namespace afterhours {

#ifdef AFTER_HOURS_USE_RAYLIB
using Color = raylib::Color;
namespace colors {
constexpr Color UI_BLACK = raylib::BLACK;
constexpr Color UI_RED = raylib::RED;
constexpr Color UI_GREEN = raylib::GREEN;
constexpr Color UI_BLUE = raylib::BLUE;
constexpr Color UI_WHITE = raylib::RAYWHITE;
constexpr Color UI_PINK = raylib::PINK;
} // namespace colors

#else
using Color = ColorType;
namespace colors {
constexpr Color UI_BLACK = {0, 0, 0, 255};
constexpr Color UI_RED = {255, 0, 0, 255};
constexpr Color UI_GREEN = {0, 255, 0, 255};
constexpr Color UI_BLUE = {0, 0, 255, 255};
constexpr Color UI_WHITE = {255, 255, 255, 255};
constexpr Color UI_PINK = {250, 200, 200, 255};
} // namespace colors

#endif

namespace colors {
static const Color red = UI_RED;
static const Color transleucent_green = Color{0, 250, 50, 5};
static const Color transleucent_red = Color{250, 0, 50, 5};
static const Color pacific_blue = Color{71, 168, 189, 255};
static const Color oxford_blue = Color{12, 27, 51, 255};
static const Color orange_soda = Color{240, 100, 73, 255};
static const Color isabelline = Color{237, 230, 227, 255};
static const Color tea_green =
    Color{166, 185, 189, 255}; // 195, 232, 189, 255};

static Color darken(const Color &color, float factor = 0.8f) {
  Color darkerColor;
  darkerColor.r = static_cast<unsigned char>(color.r * factor);
  darkerColor.g = static_cast<unsigned char>(color.g * factor);
  darkerColor.b = static_cast<unsigned char>(color.b * factor);
  darkerColor.a = color.a;
  return darkerColor;
}
} // namespace colors

struct HasColor : BaseComponent {
private:
  mutable Color color_;

public:
  using FetchFn = std::function<Color()>;
  bool is_dynamic = false;
  FetchFn fetch_fn = nullptr;

  HasColor(Color c) : color_(c) {}
  HasColor(const FetchFn &fetch)
      : color_(fetch()), is_dynamic(true), fetch_fn(fetch) {}

  Color color() const {
    if (fetch_fn) {
      color_ = fetch_fn();
    }
    return color_;
  }
  auto &set(Color col) {
    color_ = col;
    return *this;
  }
};
} // namespace afterhours
