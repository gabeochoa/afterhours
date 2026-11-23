#include <afterhours/src/plugins/impostor_atlas.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace afterhours::impostor_atlas;
using afterhours::Vector3Type;

static int checks = 0, failures = 0;
static void check(bool cond, const std::string &what) {
  ++checks;
  if (!cond) {
    ++failures;
    std::printf("  FAIL: %s\n", what.c_str());
  }
}

static float dot(Vector3Type a, Vector3Type b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static float len(Vector3Type a) { return std::sqrt(dot(a, a)); }
static bool near(float a, float b, float eps = 1e-4f) {
  return std::fabs(a - b) <= eps;
}

static bool overlaps(const AtlasPlacement &a, const AtlasPlacement &b) {
  return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height &&
         b.y < a.y + a.height;
}

int main() {
  std::vector<AtlasItem> items = {{"tree", 1024, 1024}, {"house", 512, 768},
                                  {"rock", 512, 512},   {"bush", 256, 512},
                                  {"sign", 128, 256}};
  auto layout = pack(items, 2048, 2048, 2);
  check(layout.has_value(), "pack fits");
  if (layout) {
    check(layout->placements.size() == items.size(), "one placement each");
    for (std::size_t i = 0; i < items.size(); i++) {
      const AtlasPlacement &p = layout->placements[i];
      check(p.name == items[i].name && p.width == items[i].width &&
                p.height == items[i].height,
            "placement aligned with input order");
      check(p.x >= 0 && p.y >= 0 && p.x + p.width <= 2048 &&
                p.y + p.height <= 2048,
            "placement inside atlas");
      for (std::size_t j = i + 1; j < items.size(); j++)
        check(!overlaps(p, layout->placements[j]), "no overlap");
    }
    check(layout->find("house") != nullptr &&
              layout->find("nope") == nullptr,
          "find by name");
    const AtlasPlacement *house = layout->find("house");
    if (house) {
      auto uv = house->uv(2048, 2048);
      check(near(uv.u0, house->x / 2048.f) && near(uv.u1, (house->x + 512) / 2048.f) &&
                near(uv.v1, (house->y + 768) / 2048.f),
            "uv matches pixel rect");
    }
  }

  std::vector<AtlasItem> shuffled = {items[3], items[0], items[4], items[1],
                                     items[2]};
  auto again = pack(shuffled, 2048, 2048, 2);
  check(again.has_value(), "repack fits");
  if (layout && again)
    for (const char *name : {"tree", "house", "rock", "bush", "sign"}) {
      const AtlasPlacement *a = layout->find(name), *b = again->find(name);
      check(a && b && a->x == b->x && a->y == b->y, "deterministic per name");
    }

  check(!pack({{"too-wide", 4096, 16}}, 2048, 2048).has_value(),
        "item wider than atlas fails");
  check(!pack({{"a", 1024, 1024}, {"b", 1024, 1024}, {"c", 1024, 1024}},
              2048, 1024)
             .has_value(),
        "shelves past max height fail");
  check(!pack({{"empty", 0, 16}}, 2048, 2048).has_value(),
        "empty item fails");
  check(pack({}, 64, 64).has_value(), "no items packs to an empty layout");

  {
    BillboardBasis b = billboard_basis({0, 0, 0}, {0, 0, 10}, 2.f);
    check(near(dot(b.forward, {0, 0, 1}), 1.f), "forward at eye (+z)");
    check(near(len(b.right), 1.f) && near(len(b.up), 1.f) &&
              near(len(b.forward), 1.f),
          "basis unit length");
    check(near(dot(b.right, b.up), 0.f) && near(dot(b.right, b.forward), 0.f) &&
              near(dot(b.up, b.forward), 0.f),
          "basis orthonormal");
    check(b.half_size > 2.f && near(b.half_size, 2.f * 10.f / std::sqrt(96.f)),
          "half size covers bounding sphere");
    Vector3Type corner = quad_point({0, 0, 0}, b, 1.f, 1.f);
    check(near(len(corner), b.half_size * std::sqrt(2.f), 1e-3f) &&
              near(corner.z, 0.f),
          "quad corner in the facing plane");
  }
  {
    Vector3Type eye{3.f, 4.f, 5.f};
    BillboardBasis b = billboard_basis({1, 1, 1}, eye, 1.f);
    Vector3Type to{eye.x - 1, eye.y - 1, eye.z - 1};
    check(near(dot(b.forward, to) / len(to), 1.f, 1e-5f),
          "forward at eye (diagonal)");
    check(near(dot(b.right, b.up), 0.f) && near(len(b.up), 1.f),
          "orthonormal (diagonal)");
  }
  {
    BillboardBasis b = billboard_basis({0, 0, 0}, {0, 10, 0}, 1.f);
    check(near(len(b.right), 1.f) && near(len(b.up), 1.f) &&
              near(dot(b.right, b.up), 0.f),
          "orthonormal at the pole");
  }
  {
    BillboardBasis b = billboard_basis({1, 2, 3}, {1, 2, 3}, 2.f);
    check(std::isfinite(b.half_size) && near(len(b.forward), 1.f),
          "eye at centre stays finite");
  }

  std::printf("impostor_atlas_test: %d/%d checks passed\n", checks - failures,
              checks);
  return failures == 0 ? 0 : 1;
}
