#pragma once

// Scores a house_of_cards bake against the model it came from: both are
// rendered from the same orbiting camera and compared pixel by pixel.
//
//   iou        silhouette overlap, |model & card| / |model | card|
//   color_err  mean RGB difference where both cover, 0 (same) to 1
//   bad_px     pixels that differ by more than Orbit::tolerance in any
//              channel, both renders composited on the same background:
//              0 means indistinguishable at that tolerance
//   bad_frac   bad_px over the pixels either render covers
//   bad_inner  the bad pixels more than a pixel from either silhouette:
//              what is left once one-pixel edge shifts are set aside
//   max_diff   largest single-channel difference anywhere, 0-255
//
// Shared by tools/house_of_cards/compare.cpp and tests/house_of_cards_test.

#include <cstdlib>
#include <vector>

#include <afterhours/src/plugins/house_of_cards.h>

namespace afterhours::house_of_cards {

struct Score {
  float yaw_deg = 0.f;
  float iou = 0.f;
  float color_err = 0.f;
  float bad_px = 0.f;
  float bad_frac = 0.f;
  float bad_inner = 0.f;
  int max_diff = 0;
};

struct Orbit {
  int angles = 12;
  int size = 256;
  float elevation_deg = 20.f;
  float distance = 3.f; // camera distance, in bounding radii
  int parallax_steps = 2;
  int tolerance = 8;    // per channel, out of 255
  // Lit comparison: the model is drawn with model_shader (load_lit_shader)
  // and the cards' shader must be lit with the same function. Both get
  // apply_lighting(lighting) every angle. Unset: both unlit.
  const Lighting *lighting = nullptr;
  ShaderType model_shader{};
};

namespace detail {
inline Camera3D orbit_camera(const Meta &m, float yaw_deg,
                                     const Orbit &orbit) {
  float yaw = yaw_deg * DEG2RAD, el = orbit.elevation_deg * DEG2RAD;
  float dist = m.radius * orbit.distance;
  Camera3D cam{};
  cam.position = add(m.center, {dist * std::cos(el) * std::cos(yaw),
                                dist * std::sin(el),
                                dist * std::cos(el) * std::sin(yaw)});
  cam.target = m.center;
  cam.up = {0, 1, 0};
  cam.fovy = 45.f;
  cam.projection = raylib::CAMERA_PERSPECTIVE;
  return cam;
}

// What a viewer would see: covered pixels show the render, the rest show
// this. A silhouette mismatch is then a colour difference like any other.
constexpr raylib::Color BACKGROUND = {135, 190, 230, 255};

inline int pixel_diff(raylib::Color a, raylib::Color b) {
  if (a.a <= 127) a = BACKGROUND;
  if (b.a <= 127) b = BACKGROUND;
  int dr = std::abs(a.r - b.r), dg = std::abs(a.g - b.g),
      db = std::abs(a.b - b.b);
  return dr > dg ? (dr > db ? dr : db) : (dg > db ? dg : db);
}

inline Score score_pair(const raylib::Image &a, const raylib::Image &b,
                        int tolerance) {
  auto *pa = static_cast<const raylib::Color *>(a.data);
  auto *pb = static_cast<const raylib::Color *>(b.data);
  long inter = 0, uni = 0, bad = 0, inner = 0;
  int max_diff = 0;
  double err = 0.0;
  int w = a.width, h = a.height;
  // True when the 3x3 block around i is all covered or all empty in both.
  auto away_from_edges = [&](int i) {
    int x = i % w, y = i / w;
    bool ca = pa[i].a > 127, cb = pb[i].a > 127;
    for (int dy = -1; dy <= 1; dy++)
      for (int dx = -1; dx <= 1; dx++) {
        int nx = x + dx, ny = y + dy;
        if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
        int j = ny * w + nx;
        if ((pa[j].a > 127) != ca || (pb[j].a > 127) != cb) return false;
      }
    return true;
  };
  for (int i = 0; i < w * h; i++) {
    int diff = pixel_diff(pa[i], pb[i]);
    if (diff > tolerance) {
      bad++;
      if (away_from_edges(i)) inner++;
    }
    if (diff > max_diff) max_diff = diff;
    bool ca = pa[i].a > 127, cb = pb[i].a > 127;
    if (ca || cb) uni++;
    if (!(ca && cb)) continue;
    inter++;
    err += std::abs(pa[i].r - pb[i].r) + std::abs(pa[i].g - pb[i].g) +
           std::abs(pa[i].b - pb[i].b);
  }
  Score s;
  s.bad_px = static_cast<float>(bad);
  s.bad_inner = static_cast<float>(inner);
  s.bad_frac = uni ? static_cast<float>(bad) / static_cast<float>(uni) : 0.f;
  s.max_diff = max_diff;
  s.iou = uni ? static_cast<float>(inter) / static_cast<float>(uni) : 1.f;
  s.color_err =
      inter ? static_cast<float>(err / (3.0 * 255.0 * static_cast<double>(inter)))
            : 1.f;
  return s;
}
} // namespace detail

// Where two renders disagree: red past the tolerance, grey where they agree
// on the object, black on shared background.
inline raylib::Image diff_image(const raylib::Image &a, const raylib::Image &b,
                                int tolerance) {
  raylib::Image out = raylib::GenImageColor(a.width, a.height, raylib::BLACK);
  auto *pa = static_cast<const raylib::Color *>(a.data);
  auto *pb = static_cast<const raylib::Color *>(b.data);
  auto *po = static_cast<raylib::Color *>(out.data);
  for (int i = 0; i < a.width * a.height; i++) {
    if (detail::pixel_diff(pa[i], pb[i]) > tolerance)
      po[i] = {255, 40, 40, 255};
    else if (pa[i].a > 127 || pb[i].a > 127)
      po[i] = {90, 90, 90, 255};
  }
  return out;
}

// One score per angle. If `model_shots` / `card_shots` are given, the renders
// are appended for the caller to inspect and unload.
inline std::vector<Score> compare(const ModelType &model, Cards &cards,
                                  ShaderType shader, const Orbit &orbit,
                                  std::vector<raylib::Image> *model_shots =
                                      nullptr,
                                  std::vector<raylib::Image> *card_shots =
                                      nullptr) {
  raylib::RenderTexture2D rt =
      raylib::LoadRenderTexture(orbit.size, orbit.size);
  const MatrixType identity = {1, 0, 0, 0, 0, 1, 0, 0,
                                   0, 0, 1, 0, 0, 0, 0, 1};
  std::vector<Score> scores;
  for (int i = 0; i < orbit.angles; i++) {
    float yaw = 360.f * static_cast<float>(i) / static_cast<float>(orbit.angles);
    Camera3D cam =
        detail::orbit_camera(cards.meta, yaw, orbit);

    std::vector<raylib::Shader> saved;
    if (orbit.lighting) {
      apply_lighting(orbit.model_shader, *orbit.lighting, cam.position);
      apply_lighting(shader, *orbit.lighting, cam.position);
      for (int m = 0; m < model.materialCount; m++) {
        saved.push_back(model.materials[m].shader);
        model.materials[m].shader = orbit.model_shader;
      }
    }
    raylib::BeginTextureMode(rt);
    raylib::ClearBackground({0, 0, 0, 0});
    raylib::BeginMode3D(cam);
    raylib::DrawModel(model, {0, 0, 0}, 1.f, raylib::WHITE);
    raylib::EndMode3D();
    raylib::EndTextureMode();
    for (size_t m = 0; m < saved.size(); m++)
      model.materials[m].shader = saved[m];
    raylib::Image a = detail::read_target(rt);

    raylib::BeginTextureMode(rt);
    raylib::ClearBackground({0, 0, 0, 0});
    raylib::BeginMode3D(cam);
    draw_instanced(cards, shader, &identity, 1, cam, orbit.parallax_steps);
    raylib::EndMode3D();
    raylib::EndTextureMode();
    raylib::Image b = detail::read_target(rt);

    Score s = detail::score_pair(a, b, orbit.tolerance);
    s.yaw_deg = yaw;
    scores.push_back(s);
    if (model_shots) model_shots->push_back(a);
    else raylib::UnloadImage(a);
    if (card_shots) card_shots->push_back(b);
    else raylib::UnloadImage(b);
  }
  raylib::UnloadRenderTexture(rt);
  return scores;
}

inline Score mean(const std::vector<Score> &scores) {
  Score m;
  float n = static_cast<float>(scores.size());
  for (const Score &s : scores) {
    m.iou += s.iou / n;
    m.color_err += s.color_err / n;
    m.bad_px += s.bad_px / n;
    m.bad_inner += s.bad_inner / n;
    m.bad_frac += s.bad_frac / n;
    if (s.max_diff > m.max_diff) m.max_diff = s.max_diff;
  }
  return m;
}

inline Score worst(const std::vector<Score> &scores) {
  Score w = scores.front();
  for (const Score &s : scores)
    if (s.iou < w.iou) w = s;
  return w;
}

} // namespace afterhours::house_of_cards
