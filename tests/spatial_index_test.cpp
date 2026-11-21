#include <afterhours/src/spatial_index.h>

#include <algorithm>
#include <cstdio>
#include <random>
#include <vector>

using namespace afterhours;
using Index = SpatialIndex<int>;
using Entry = Index::Entry;

static int checks = 0, failures = 0;
static void check(bool c, const char *e, int l) {
  checks++;
  if (!c) { failures++; std::fprintf(stderr, "  FAIL: %s (line %d)\n", e, l); }
}
#define CHECK(...) check((__VA_ARGS__), #__VA_ARGS__, __LINE__)

static std::vector<int> sorted(std::vector<int> v) {
  std::sort(v.begin(), v.end());
  return v;
}
static std::vector<int> brute_radius(const std::vector<Entry> &es, Index::Point c, float r) {
  std::vector<int> out;
  for (auto &e : es) {
    float dx = e.pos.x - c.x, dy = e.pos.y - c.y;
    if (dx * dx + dy * dy <= r * r) out.push_back(e.id);
  }
  return sorted(out);
}
static std::vector<int> brute_box(const std::vector<Entry> &es, Index::Point mn, Index::Point mx) {
  std::vector<int> out;
  for (auto &e : es)
    if (e.pos.x >= mn.x && e.pos.x <= mx.x && e.pos.y >= mn.y && e.pos.y <= mx.y)
      out.push_back(e.id);
  return sorted(out);
}

int main() {
  // Empty index.
  {
    Index idx;
    CHECK(idx.empty());
    CHECK(idx.query_radius({0, 0}, 10).empty());
    CHECK(idx.query_box({0, 0}, {10, 10}).empty());
  }
  // Static + dynamic queried together; rebuild_dynamic replaces, not appends.
  {
    Index idx(10.f);
    idx.rebuild_static(std::vector<Entry>{{1, {0, 0}}, {2, {5, 0}}});
    idx.rebuild_dynamic(std::vector<Entry>{{3, {0, 5}}});
    CHECK(sorted(idx.query_radius({0, 0}, 5)) == std::vector<int>({1, 2, 3}));
    CHECK(sorted(idx.query_box({-1, -1}, {5, 0})) == std::vector<int>({1, 2}));
    idx.rebuild_dynamic(std::vector<Entry>{{4, {100, 100}}});
    CHECK(sorted(idx.query_radius({0, 0}, 5)) == std::vector<int>({1, 2}));
    CHECK(idx.size() == 3 && idx.static_size() == 2 && idx.dynamic_size() == 1);
    idx.clear_dynamic();
    CHECK(idx.size() == 2);
  }
  // Boundary: points exactly on radius / box edge / cell boundaries included.
  {
    Index idx(10.f);
    idx.rebuild(std::vector<Entry>{{1, {10, 0}}, {2, {20, 20}}, {3, {-10, -10}}, {4, {11, 0}}});
    CHECK(sorted(idx.query_radius({0, 0}, 10)) == std::vector<int>({1}));
    CHECK(sorted(idx.query_box({10, 0}, {20, 20})) == std::vector<int>({1, 2, 4}));
    CHECK(idx.query_radius({0, 0}, -1).empty());
    CHECK(idx.query_box({5, 5}, {0, 0}).empty());
  }
  // Randomized: grid equals brute force over both layers.
  {
    std::mt19937 rng(223);
    std::uniform_real_distribution<float> p(-100.f, 100.f), q(-80.f, 80.f);
    std::uniform_real_distribution<float> rad(0.f, 60.f);
    for (float cell : {1.f, 7.5f, 32.f, 64.f}) {
      std::vector<Entry> statik, dyn, all;
      for (int i = 0; i < 200; i++) statik.push_back({i, {p(rng), p(rng)}});
      for (int i = 200; i < 400; i++) dyn.push_back({i, {p(rng), p(rng)}});
      all.insert(all.end(), statik.begin(), statik.end());
      all.insert(all.end(), dyn.begin(), dyn.end());
      Index idx(cell);
      idx.rebuild_static(statik);
      idx.rebuild_dynamic(dyn);
      for (int t = 0; t < 100; t++) {
        Index::Point c{q(rng), q(rng)};
        float r = rad(rng);
        CHECK(sorted(idx.query_radius(c, r)) == brute_radius(all, c, r));
        Index::Point mn{q(rng), q(rng)};
        Index::Point mx{mn.x + rad(rng), mn.y + rad(rng)};
        CHECK(sorted(idx.query_box(mn, mx)) == brute_box(all, mn, mx));
      }
    }
  }
  std::printf("spatial_index_test: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
