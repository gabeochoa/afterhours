
#pragma once

#include <algorithm>
#include <cmath>

#include "../core/base_component.h"
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
}  // namespace colors

#else
using Color = ColorType;
namespace colors {
constexpr Color UI_BLACK = {0, 0, 0, 255};
constexpr Color UI_RED = {255, 0, 0, 255};
constexpr Color UI_GREEN = {0, 255, 0, 255};
constexpr Color UI_BLUE = {0, 0, 255, 255};
constexpr Color UI_WHITE = {255, 255, 255, 255};
constexpr Color UI_PINK = {250, 200, 200, 255};
}  // namespace colors

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
    Color{166, 185, 189, 255};  // 195, 232, 189, 255};

static Color darken(const Color &color, const float factor = 0.8f) {
    Color darkerColor;
    darkerColor.r = static_cast<unsigned char>(color.r * factor);
    darkerColor.g = static_cast<unsigned char>(color.g * factor);
    darkerColor.b = static_cast<unsigned char>(color.b * factor);
    darkerColor.a = color.a;
    return darkerColor;
}

static Color increase(const Color &color, const int factor = 10) {
    Color darkerColor;
    darkerColor.r = static_cast<unsigned char>(color.r + factor);
    darkerColor.g = static_cast<unsigned char>(color.g + factor);
    darkerColor.b = static_cast<unsigned char>(color.b + factor);
    darkerColor.a = color.a;
    return darkerColor;
}

static Color set_opacity(const Color &color, const unsigned char alpha) {
    Color transparentColor = color;
    transparentColor.a = alpha;
    return transparentColor;
}

static Color opacity_pct(const Color &color, const float percentage) {
    Color transparentColor = color;
    transparentColor.a =
        static_cast<unsigned char>(255 * std::clamp(percentage, 0.0f, 1.0f));
    return transparentColor;
}

// Additional color utility functions
static Color get_opposite(const Color &color) {
    Color opposite;
    opposite.r = static_cast<unsigned char>(255 - color.r);
    opposite.g = static_cast<unsigned char>(255 - color.g);
    opposite.b = static_cast<unsigned char>(255 - color.b);
    opposite.a = color.a;
    return opposite;
}

static unsigned char comp_min(const Color &a) {
    return static_cast<unsigned char>(std::min({a.r, a.g, a.b}));
}

static unsigned char comp_max(const Color &a) {
    return static_cast<unsigned char>(std::max({a.r, a.g, a.b}));
}

static bool is_empty(const Color &c) {
    return c.r == 0 && c.g == 0 && c.b == 0 && c.a == 0;
}

#ifdef AFTER_HOURS_USE_RAYLIB
// HSL conversion functions (requires Vector3 from raylib)
static raylib::Vector3 to_hsl(const Color color) {
    const unsigned char cmax = comp_max(color);
    const unsigned char cmin = comp_min(color);
    const unsigned char delta = cmax - cmin;

    constexpr float COLOR_EPSILON = 0.000001f;
    raylib::Vector3 hsl = {0.f, 0.f, (cmax + cmin) / 2.f};

    if (std::abs(static_cast<float>(delta)) <= COLOR_EPSILON) {
        return hsl;
    }

    if (std::abs(static_cast<float>(cmax - color.r)) <= COLOR_EPSILON) {
        hsl.x = static_cast<float>(
            std::fmod(static_cast<float>(color.g - color.b) / delta, 6.f));
    } else if (std::abs(static_cast<float>(cmax - color.g)) <= COLOR_EPSILON) {
        hsl.x = (static_cast<float>(color.b - color.r) / delta) + 2.f;
    } else {
        hsl.x = (static_cast<float>(color.r - color.g) / delta) + 4.f;
    }

    hsl.y = static_cast<float>(delta / (1.f - std::abs(2.f * hsl.z - 1.f)));
    hsl.x /= 6.f;
    hsl.x = std::fmod(hsl.x + 1.f, 1.f);
    return hsl;
}

static Color to_rgb(const raylib::Vector3 &hsl) {
    const float k = hsl.x * 6.f;
    const float c = (1.f - std::abs(2.f * hsl.z - 1.f)) * hsl.y;
    const float x = c * (1.f - std::abs(std::fmod(k, 2.f) - 1.f));
    const float m = hsl.z - c / 2.f;
    const int d = static_cast<int>(std::floor(k));

    raylib::Vector3 rgb;

    switch (d) {
        case 0:
            rgb = {c, x, 0.f};
            break;
        case 1:
            rgb = {x, c, 0.f};
            break;
        case 2:
            rgb = {0.f, c, x};
            break;
        case 3:
            rgb = {0.f, x, c};
            break;
        case 4:
            rgb = {x, 0.f, c};
            break;
        default:
            rgb = {c, 0.f, x};
            break;
    }

    return Color{
        static_cast<unsigned char>(255 * (rgb.x + m)),
        static_cast<unsigned char>(255 * (rgb.y + m)),
        static_cast<unsigned char>(255 * (rgb.z + m)),
        static_cast<unsigned char>(255),
    };
}

static Color get_highlighted(const Color &color) {
    auto hsl = to_hsl(color);
    hsl.z = (hsl.z + 0.01f);
    return to_rgb(hsl);
}
#else
// Non-raylib versions - would need Vector3Type defined
// For now, these are stubs that return default values
struct Vector3Type {
    float x, y, z;
};

static Vector3Type to_hsl(const Color) {
    log_warn("HSL conversion not supported without raylib");
    return Vector3Type{0, 0, 0};
}

static Color to_rgb(const Vector3Type &) {
    log_warn("RGB conversion not supported without raylib");
    return Color{0, 0, 0, 255};
}

static Color get_highlighted(const Color &color) {
    log_warn("get_highlighted not supported without raylib");
    return color;
}
#endif
}  // namespace colors

struct HasColor : BaseComponent {
   private:
    mutable Color color_;

   public:
    using FetchFn = std::function<Color()>;
    bool is_dynamic = false;
    FetchFn fetch_fn = nullptr;

    HasColor(const Color c) : color_(c) {}
    HasColor(const FetchFn &fetch)
        : color_(fetch()), is_dynamic(true), fetch_fn(fetch) {}

    Color color() const {
        if (fetch_fn) {
            color_ = fetch_fn();
        }
        return color_;
    }
    auto &set(const Color col) {
        color_ = col;
        return *this;
    }
};
}  // namespace afterhours
