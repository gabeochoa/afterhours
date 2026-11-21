#pragma once

// One view of the world: camera, render-target size, derived frustum and
// a LOD scale, kept together so culling and LOD agree on the same frame.
//
//   RenderView view = make_render_view(camera, screen_w, screen_h);
//   if (!view.intersects_box(min, max)) continue;
//   int level = lod.update(view.lod_distance(dist), thresholds);
//
// LOD scale is target height relative to 1080p: thresholds authored at
// 1080p scale with resolution, so lod_distance() divides by it. The full
// world post-process chain is deferred: Effect/BlurPass have no common
// pass interface and own GPU targets, so a real chain needs that design
// first. PostChain below is only the trivially composable order runner:
// wrap existing passes as callables and they run in insertion order.

#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <span>
#include <utility>
#include <vector>

#include "../drawing_helpers_3d.h"
#include "frustum.h"
#include "lod.h"

namespace afterhours {

struct RenderView {
  Camera3D camera{};
  int target_w = 0;
  int target_h = 0;
  float near_dist = 0.01f;
  float far_dist = 1000.f;
  float lod_scale = 1.f;
  Frustum frustum{};

  [[nodiscard]] float aspect() const {
    return target_h > 0 ? static_cast<float>(target_w) / static_cast<float>(target_h) : 0.f;
  }
  [[nodiscard]] bool contains_point(Vector3Type p) const { return frustum.contains_point(p); }
  [[nodiscard]] bool intersects_sphere(Vector3Type c, float r) const {
    return frustum.intersects_sphere(c, r);
  }
  [[nodiscard]] bool intersects_box(Vector3Type mn, Vector3Type mx) const {
    return frustum.intersects_box(mn, mx);
  }
  // Distance as LOD thresholds see it: larger targets use farther thresholds.
  [[nodiscard]] float lod_distance(float d) const {
    return d / (lod_scale > 0.f ? lod_scale : 1.f);
  }
  int lod_level(Lod &lod, float distance, std::span<const float> thresholds,
                float margin = 0.15f) const {
    return lod.update(lod_distance(distance), thresholds, margin);
  }
  int lod_level(Lod &lod, float distance, std::initializer_list<float> thresholds,
                float margin = 0.15f) const {
    return lod_level(lod, distance,
                     std::span<const float>(thresholds.begin(), thresholds.size()), margin);
  }

  // 8 corners: near quad then far quad, each bottom-left, bottom-right,
  // top-right, top-left as seen by the camera.
  [[nodiscard]] std::array<Vector3Type, 8> corners() const {
    using namespace detail_frustum;
    V pos{camera.position.x, camera.position.y, camera.position.z};
    V fwd = normalize(sub(V{camera.target.x, camera.target.y, camera.target.z}, pos));
    V r = normalize(cross(fwd, V{camera.up.x, camera.up.y, camera.up.z}));
    V u = cross(r, fwd);
    float hn, wn, hf, wf;
    if (camera.projection == camera::ORTHOGRAPHIC) {
      hf = hn = camera.fovy * 0.5f;
      wf = wn = hn * aspect();
    } else {
      float t = std::tan(camera.fovy * 0.5f * DEG2RAD);
      hn = t * near_dist; wn = hn * aspect();
      hf = t * far_dist;  wf = hf * aspect();
    }
    std::array<Vector3Type, 8> out{};
    auto put = [&](int i, V c, float hw, float hh, float sx, float sy) {
      V p = add(c, add(mul(r, sx * hw), mul(u, sy * hh)));
      out[static_cast<size_t>(i)] = Vector3Type{p.x, p.y, p.z};
    };
    V cn = add(pos, mul(fwd, near_dist)), cf = add(pos, mul(fwd, far_dist));
    put(0, cn, wn, hn, -1, -1); put(1, cn, wn, hn, 1, -1);
    put(2, cn, wn, hn, 1, 1);   put(3, cn, wn, hn, -1, 1);
    put(4, cf, wf, hf, -1, -1); put(5, cf, wf, hf, 1, -1);
    put(6, cf, wf, hf, 1, 1);   put(7, cf, wf, hf, -1, 1);
    return out;
  }
};

inline RenderView make_render_view(const Camera3D &cam, int target_w, int target_h,
                                   float near_dist = 0.01f, float far_dist = 1000.f) {
  RenderView v;
  v.camera = cam;
  v.target_w = target_w < 0 ? 0 : target_w;
  v.target_h = target_h < 0 ? 0 : target_h;
  v.near_dist = near_dist;
  v.far_dist = far_dist;
  v.lod_scale = v.target_h > 0 ? static_cast<float>(v.target_h) / 1080.f : 1.f;
  v.frustum = make_frustum(cam, v.aspect(), near_dist, far_dist);
  return v;
}

// Ordered runner for passes the caller already owns (e.g. BlurPass::apply
// wrapped in a lambda). Empty chain is a no-op; steps run in add order.
struct PostChain {
  std::vector<std::function<void()>> steps;
  void add(std::function<void()> step) { steps.push_back(std::move(step)); }
  void clear() { steps.clear(); }
  [[nodiscard]] std::size_t size() const { return steps.size(); }
  void run() const { for (const auto &s : steps) if (s) s(); }
};

} // namespace afterhours
