
#define FMT_HEADER_ONLY
#define AFTER_HOURS_USE_NONE
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/instance_field.h>

#include <cmath>
#include <cstdio>
#include <string>

using namespace afterhours;

static int checks = 0, failures = 0;
static void check(bool c, const std::string &w) {
  ++checks;
  if (!c) { ++failures; std::printf("  FAIL: %s\n", w.c_str()); }
}
static bool near(float a, float b) { return std::fabs(a - b) < 1e-5f; }

int main() {
  {
    auto a = scatter_field({0, 2, 0}, {100, 2, 100}, 200, 42u, 0.5f, 1.5f);
    auto b = scatter_field({0, 2, 0}, {100, 2, 100}, 200, 42u, 0.5f, 1.5f);
    auto c = scatter_field({0, 2, 0}, {100, 2, 100}, 200, 7u, 0.5f, 1.5f);
    check(a == b, "same seed scatters identically");
    check(!(a == c), "different seed scatters differently");
    check(a.size() == 200, "scatter count");
    bool in = true;
    for (auto &t : a)
      in &= t.pos.x >= 0 && t.pos.x < 100 && t.pos.z >= 0 && t.pos.z < 100 &&
            near(t.pos.y, 2.f) && t.scale.x >= 0.5f && t.scale.x < 1.5f;
    check(in, "scatter stays in bounds at ground y with scale range");
    RNGStream r1(9), r2(9);
    check(scatter_field({0,0,0},{10,0,10},10,r1) == scatter_field({0,0,0},{10,0,10},10,r2),
          "stream overload is deterministic too");
    check(scatter_field({0,0,0},{1,0,1},0,1u).empty(), "zero count scatters nothing");
  }

  {
    check(field_chunk_key_for({0, 0, 0}, 16.f).x == 0, "origin in chunk 0");
    check(field_chunk_key_for({15.9f, 0, 0}, 16.f).x == 0, "just inside stays");
    check(field_chunk_key_for({16.f, 0, 0}, 16.f).x == 1, "on boundary goes next");
    check(field_chunk_key_for({-0.1f, 0, 0}, 16.f).x == -1, "negative floors down");
    check(field_chunk_key_for({-16.f, 0, -32.f}, 16.f).z == -2, "negative boundary exact");
  }

  InstanceField field;
  {
    std::vector<InstanceTransform> ts = {
        {{1, 0, 1}, 0, {1,1,1}}, {{20, 0, 1}, 0, {1,1,1}},
        {{2, 0, 2}, 0, {1,1,1}}, {{-1, 0, -1}, 0, {1,1,1}}};
    field.build(ts, 16.f);
    check(field.chunks.size() == 3, "three chunks");
    check(field.count() == 4, "all instances kept");
    check(field.chunk_at({1,0,1}) && field.chunk_at({1,0,1})->instances.size() == 2,
          "chunk holds its two");
    check(field.chunk_at({500,0,500}) == nullptr, "empty chunk is null");
    check(field.chunks[0].key.x == -1, "chunks sorted by (z,x)");
  }

  {
    check(field_lod_for(10.f, {.full=40.f,.reduced=90.f}) == FieldLod::Full, "near is full");
    check(field_lod_for(40.f, {.full=40.f,.reduced=90.f}) == FieldLod::Full, "full edge inclusive");
    check(field_lod_for(50.f, {.full=40.f,.reduced=90.f}) == FieldLod::Reduced, "mid is reduced");
    check(field_lod_for(90.f, {.full=40.f,.reduced=90.f}) == FieldLod::Reduced, "reduced edge inclusive");
    check(field_lod_for(91.f, {.full=40.f,.reduced=90.f}) == FieldLod::Skip, "far is skip");

    auto at_cam = field.selected({8, 0, 8}, {.full=40.f,.reduced=90.f,.density=0.25f});
    check(at_cam.size() == 4, "camera near: all three chunks within full band");
  }

  {
    std::vector<InstanceTransform> ts;
    for (int i = 0; i < 100; i++) ts.push_back({{1.f + (i % 10), 0, 1.f + (i / 10)}, 0, {1,1,1}});
    InstanceField f; f.build(ts, 16.f);
    auto full = f.selected({8,0,8}, {.full=40.f,.reduced=90.f,.density=0.25f});
    check(full.size() == 100, "full band keeps all");
    auto far = f.selected({1000,0,8}, {.full=40.f,.reduced=90.f,.density=0.25f});
    check(far.empty(), "skip band keeps none");
    auto mid1 = f.selected({58,0,8}, {.full=40.f,.reduced=90.f,.density=0.25f});
    auto mid2 = f.selected({60,0,9}, {.full=40.f,.reduced=90.f,.density=0.25f});
    check(mid1 == mid2, "reduced subset stable as camera moves in band");
    check(mid1.size() > 10 && mid1.size() < 40, "reduced subset near density");
    auto none = f.selected({58,0,8}, {.full=40.f,.reduced=90.f,.density=0.f});
    check(none.empty(), "density 0 keeps none");
    auto all = f.selected({58,0,8}, {.full=40.f,.reduced=90.f,.density=1.f});
    check(all.size() == 100, "density 1 keeps all");
  }

  std::printf("instance_field_test: %d/%d checks passed\n", checks - failures, checks);
  return failures ? 1 : 0;
}
