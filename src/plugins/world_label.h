#pragma once

// World-space labels: nameplates, order bubbles, debug tags.
//   WorldLabel label{.text = "Soup!", .anchor = {x, y, z}};
//   draw_world_label(font, camera, label, screen_w, screen_h);

#include <cmath>
#include <string>

#include "../core/base_component.h"
#include "../drawing_helpers.h"
#include "../drawing_helpers_3d.h"
#include "color.h"

namespace afterhours {

struct WorldLabel : public BaseComponent {
  std::string text;
  Vector3Type anchor{};
  float fade_near = 20.f; // full alpha at or inside this distance
  float fade_far = 60.f;  // invisible at or past this distance
  float font_size = 20.f;
  float rise = 0.f; // world units above the anchor (a nameplate's head room)
  Color color{255, 255, 255, 255};
};

struct WorldLabelProjection {
  bool visible = false;
  Vector2Type screen{};   // anchor on screen, before the label is centred
  RectangleType rect{};   // label box: centred on screen.x, bottom at screen.y
  float distance = 0.f;   // camera-to-anchor distance, world units
  float alpha = 0.f;      // 0..1 from the fade range
};

// Linear fade: 1 up to fade_near, 0 from fade_far on. A collapsed range
// (far <= near) is a step at near, so a typo hides far labels, never all.
[[nodiscard]] inline float world_label_alpha(float distance, float fade_near,
                                             float fade_far) {
  if (distance <= fade_near) return 1.f;
  if (fade_far <= fade_near || distance >= fade_far) return 0.f;
  return 1.f - (distance - fade_near) / (fade_far - fade_near);
}

// Same projection raylib's GetWorldToScreen does in BeginMode3D (and
// make_frustum's planes describe), spelled out so every backend — none
// included — agrees. Points behind the camera are culled, never mirrored.
[[nodiscard]] inline WorldLabelProjection project_world_label(
    const Camera3D &cam, Vector3Type anchor, float viewport_w,
    float viewport_h, Vector2Type label_size = {0.f, 0.f}) {
  WorldLabelProjection out;
  if (viewport_w <= 0.f || viewport_h <= 0.f) return out;

  auto sub = [](Vector3Type a, Vector3Type b) {
    return Vector3Type{a.x - b.x, a.y - b.y, a.z - b.z};
  };
  auto dot = [](Vector3Type a, Vector3Type b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
  };
  auto cross = [](Vector3Type a, Vector3Type b) {
    return Vector3Type{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                       a.x * b.y - a.y * b.x};
  };
  Vector3Type fwd = sub(cam.target, cam.position);
  float flen = std::sqrt(dot(fwd, fwd));
  if (flen <= 0.f) return out;
  fwd = {fwd.x / flen, fwd.y / flen, fwd.z / flen};
  Vector3Type right = cross(fwd, cam.up);
  float rlen = std::sqrt(dot(right, right));
  if (rlen <= 0.f) return out; // looking straight along up: no screen basis
  right = {right.x / rlen, right.y / rlen, right.z / rlen};
  Vector3Type up = cross(right, fwd);

  Vector3Type rel = sub(anchor, cam.position);
  float x = dot(rel, right), y = dot(rel, up), z = dot(rel, fwd);
  out.distance = std::sqrt(dot(rel, rel));
  if (z <= 0.f) return out; // behind (or on) the camera plane

  float aspect = viewport_w / viewport_h;
  float ndc_x, ndc_y;
  if (cam.projection == camera::ORTHOGRAPHIC) {
    float hh = cam.fovy * 0.5f;
    if (hh <= 0.f) return out;
    ndc_x = x / (hh * aspect);
    ndc_y = y / hh;
  } else {
    float tv = std::tan(cam.fovy * 0.5f * DEG2RAD);
    if (tv <= 0.f) return out;
    ndc_x = x / (z * tv * aspect);
    ndc_y = y / (z * tv);
  }
  if (ndc_x < -1.f || ndc_x > 1.f || ndc_y < -1.f || ndc_y > 1.f) return out;

  out.screen = {(ndc_x * 0.5f + 0.5f) * viewport_w,
                (0.5f - ndc_y * 0.5f) * viewport_h};
  out.rect = {out.screen.x - label_size.x * 0.5f, out.screen.y - label_size.y,
              label_size.x, label_size.y};
  out.visible = true;
  return out;
}

// Draws the label's text; returns false when culled or fully faded, so the
// caller can skip its own bubble/icon pass for the same anchor.
inline bool draw_world_label(const Font font, const Camera3D &cam,
                             const WorldLabel &label, float viewport_w,
                             float viewport_h) {
  if (label.text.empty()) return false;
  Vector3Type anchor{label.anchor.x, label.anchor.y + label.rise,
                     label.anchor.z};
  Vector2Type size =
      measure_text(font, label.text.c_str(), label.font_size, 1.f);
  WorldLabelProjection p =
      project_world_label(cam, anchor, viewport_w, viewport_h, size);
  p.alpha = world_label_alpha(p.distance, label.fade_near, label.fade_far);
  if (!p.visible || p.alpha <= 0.f) return false;
  Color c = label.color;
  c.a = static_cast<unsigned char>(c.a * p.alpha);
  draw_text_centered(font, label.text.c_str(), p.screen.x, p.rect.y,
                     label.font_size, c);
  return true;
}

} // namespace afterhours
