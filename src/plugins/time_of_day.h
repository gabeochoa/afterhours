#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace afterhours::time_of_day {

struct Rgb {
  float r = 1.f, g = 1.f, b = 1.f;
};

struct Keyframe {
  float hour = 0.f;
  Rgb sun_color{};
  float sun_intensity = 0.f;
  Rgb ambient_color{0.2f, 0.25f, 0.4f};
  float ambient_intensity = 0.15f;
  Rgb fog_color{0.02f, 0.03f, 0.08f};
  float fog_density = 0.02f;
};

struct Lighting {
  Rgb sun_color{};
  float sun_intensity = 0.f;
  struct Dir {
    float x = 0.f, y = 1.f, z = 0.f;
  };
  Dir sun_direction{};
  float sun_elevation_deg = -90.f;
  Rgb ambient_color{};
  float ambient_intensity = 0.f;
  Rgb fog_color{};
  float fog_density = 0.f;
};

inline std::vector<Keyframe> default_day_cycle() {
  return {
      {0.f, {0.6f, 0.7f, 1.f}, 0.f, {0.12f, 0.16f, 0.3f}, 0.12f, {0.015f, 0.02f, 0.06f}, 0.02f},
      {5.f, {0.7f, 0.75f, 1.f}, 0.f, {0.14f, 0.18f, 0.32f}, 0.14f, {0.02f, 0.03f, 0.08f}, 0.02f},
      {6.5f, {1.f, 0.6f, 0.35f}, 0.35f, {0.35f, 0.3f, 0.35f}, 0.3f, {0.35f, 0.28f, 0.25f}, 0.015f},
      {12.f, {1.f, 0.96f, 0.88f}, 1.f, {0.55f, 0.6f, 0.7f}, 0.55f, {0.75f, 0.82f, 0.95f}, 0.008f},
      {17.5f, {1.f, 0.55f, 0.3f}, 0.4f, {0.35f, 0.3f, 0.3f}, 0.35f, {0.5f, 0.38f, 0.28f}, 0.015f},
      {19.f, {0.8f, 0.45f, 0.5f}, 0.05f, {0.2f, 0.18f, 0.3f}, 0.2f, {0.1f, 0.08f, 0.18f}, 0.02f},
  };
}

struct TimeOfDay {
  float hour = 10.f;
  float sunrise_hour = 6.f;
  float sunset_hour = 18.f;
  float max_elevation_deg = 65.f;
  float tilt = 0.35f;
  std::vector<Keyframe> keys = default_day_cycle();

  void advance(float dt_seconds, float hours_per_second = 1.f) {
    hour = wrap(hour + dt_seconds * hours_per_second);
  }

  [[nodiscard]] Lighting evaluate() const {
    Lighting out;
    const Keyframe k = sample(hour);
    out.sun_color = k.sun_color;
    out.sun_intensity = k.sun_intensity;
    out.ambient_color = k.ambient_color;
    out.ambient_intensity = k.ambient_intensity;
    out.fog_color = k.fog_color;
    out.fog_density = k.fog_density;
    sun_position(out);
    return out;
  }

 private:
  static float wrap(float h) {
    h = std::fmod(h, 24.f);
    return h < 0.f ? h + 24.f : h;
  }

  [[nodiscard]] Keyframe sample(float h) const {
    h = wrap(h);
    if (keys.empty()) return {};
    size_t i = 0;
    while (i + 1 < keys.size() && keys[i + 1].hour <= h) ++i;
    const Keyframe &a = keys[i];
    const Keyframe &b = keys[(i + 1) % keys.size()];
    float span = b.hour - a.hour;
    if (span <= 0.f) span += 24.f;
    float f = (h - a.hour) / span;
    if (f < 0.f) f = 0.f;
    if (f > 1.f) f = 1.f;
    auto lerp = [](float u, float v, float t) { return u + (v - u) * t; };
    auto lerp_rgb = [&lerp](Rgb u, Rgb v, float t) {
      return Rgb{lerp(u.r, v.r, t), lerp(u.g, v.g, t), lerp(u.b, v.b, t)};
    };
    return {a.hour + span * f,
            lerp_rgb(a.sun_color, b.sun_color, f),
            lerp(a.sun_intensity, b.sun_intensity, f),
            lerp_rgb(a.ambient_color, b.ambient_color, f),
            lerp(a.ambient_intensity, b.ambient_intensity, f),
            lerp_rgb(a.fog_color, b.fog_color, f),
            lerp(a.fog_density, b.fog_density, f)};
  }

  void sun_position(Lighting &out) const {
    constexpr float PI = 3.14159265f;
    const float day_len = sunset_hour - sunrise_hour;
    float f = (hour - sunrise_hour) / day_len;
    const float elev = max_elevation_deg * std::sin(PI * f);
    const float az = PI * f;
    const float el = elev * PI / 180.f;
    float x = std::cos(az) * std::cos(el);
    float y = std::sin(el);
    float z = tilt * std::sin(az) * std::cos(el);
    const float len = std::sqrt(x * x + y * y + z * z);
    if (len > 0.f) {
      out.sun_direction = {x / len, y / len, z / len};
    }
    out.sun_elevation_deg = elev;
  }
};

}
