
#define FMT_HEADER_ONLY
#define AFTER_HOURS_USE_NONE
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/render_view.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace afterhours;

static int checks = 0, failures = 0;
static void check(bool c, const std::string &w) {
  ++checks;
  if (!c) { ++failures; std::printf("  FAIL: %s\n", w.c_str()); }
}

static bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }
static bool near_v(Vector3Type a, float x, float y, float z) {
  return near(a.x, x) && near(a.y, y) && near(a.z, z);
}

int main() {
  Camera3D cam{};
  cam.position = {0, 0, 0}; cam.target = {0, 0, -1}; cam.up = {0, 1, 0};
  cam.fovy = 90.f; cam.projection = camera::PERSPECTIVE;

  {
    RenderView v = make_render_view(cam, 200, 100, 1.f, 1000.f);
    check(near(v.aspect(), 2.f), "aspect from target");
    check(v.target_w == 200 && v.target_h == 100, "target stored");
    auto c = v.corners();
    check(near_v(c[0], -2, -1, -1), "near bottom-left exact");
    check(near_v(c[1], 2, -1, -1), "near bottom-right exact");
    check(near_v(c[2], 2, 1, -1), "near top-right exact");
    check(near_v(c[3], -2, 1, -1), "near top-left exact");
    check(near_v(c[4], -2000, -1000, -1000), "far bottom-left exact");
    check(near_v(c[6], 2000, 1000, -1000), "far top-right exact");
    check(v.contains_point({0, 0, -10}), "ahead inside via view");
    check(!v.contains_point({0, 0, 10}), "behind outside via view");
    check(!v.contains_point({0, 0, -2000}), "past far outside via view");
    for (auto &p : c) check(v.frustum.contains_point(p) || [&]{
      float m=1e9f; for (auto &pl: v.frustum.planes) m=std::fmin(m, std::fabs(pl.distance(p.x,p.y,p.z))); return m < 1e-3f; }(), "corner on/inside frustum");
  }

  {
    Camera3D o = cam; o.projection = camera::ORTHOGRAPHIC; o.fovy = 4.f;
    RenderView v = make_render_view(o, 200, 100, 1.f, 10.f);
    auto c = v.corners();
    check(near_v(c[0], -4, -2, -1), "ortho near bottom-left exact");
    check(near_v(c[6], 4, 2, -10), "ortho far top-right exact");
    check(v.contains_point({3.9f, 0, -5}), "ortho inside");
    check(!v.contains_point({4.1f, 0, -5}), "ortho outside side");
  }

  {
    check(near(make_render_view(cam, 1920, 1080).lod_scale, 1.f), "1080p lod_scale 1");
    check(near(make_render_view(cam, 960, 540).lod_scale, 0.5f), "540p lod_scale 0.5");
    check(near(make_render_view(cam, 3840, 2160).lod_scale, 2.f), "2160p lod_scale 2");
    RenderView v = make_render_view(cam, 3840, 2160);
    check(near(v.lod_distance(100.f), 50.f), "lod_distance divides by scale");
    Lod lod;
    check(v.lod_level(lod, 50.f, {30.f}) == 0, "lod_level holds at scaled distance");
    check(v.lod_level(lod, 100.f, {30.f}) == 1, "lod_level advances at scaled distance");
    RenderView empty = make_render_view(cam, 0, 0);
    check(near(empty.aspect(), 0.f) && near(empty.lod_scale, 1.f), "empty target safe defaults");
    check(near(empty.lod_distance(10.f), 10.f), "empty target lod passthrough");
  }

  {
    std::vector<int> order;
    PostChain chain;
    chain.run();
    check(order.empty(), "empty chain no-op");
    chain.add([&]{ order.push_back(1); });
    chain.add([&]{ order.push_back(2); });
    chain.add(nullptr);
    chain.add([&]{ order.push_back(3); });
    check(chain.size() == 4, "chain size");
    chain.run();
    check((order == std::vector<int>{1, 2, 3}), "chain runs in insertion order, null skipped");
    chain.clear();
    chain.run();
    check(order.size() == 3, "cleared chain runs nothing");
  }

  std::printf("render_view_test: %d/%d checks passed\n", checks - failures, checks);
  return failures ? 1 : 0;
}
