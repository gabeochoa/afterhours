#define FMT_HEADER_ONLY
#define AFTER_HOURS_USE_NONE
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/proximity_pool.h>

#include <cstdio>
#include <string>
#include <vector>

using namespace afterhours;

static int checks = 0;
static int failures = 0;

static void check(bool cond, const std::string &what) {
  ++checks;
  if (!cond) {
    ++failures;
    std::printf("  FAIL: %s\n", what.c_str());
  }
}

static bool at(const Vector3Type &p, float x, float y, float z) {
  return p.x == x && p.y == y && p.z == z;
}

struct Counters {
  int acquires = 0, releases = 0, factory_calls = 0;
  std::vector<Vector3Type> acquire_positions;
  std::vector<size_t> acquire_indices;

  void reset() {
    acquires = releases = factory_calls = 0;
    acquire_positions.clear();
    acquire_indices.clear();
  }
};

static void wire(ProximityPool &pool, Counters &c) {
  pool.set_on_acquire([&](Entity &, const Vector3Type &pos, size_t i) {
    ++c.acquires;
    c.acquire_positions.push_back(pos);
    c.acquire_indices.push_back(i);
  });
  pool.set_on_release([&](Entity &) { ++c.releases; });
}

int main() {
  {
    EntityCollection col;
    Counters c;
    ProximityPool pool(col, 2, [&](Entity &) { ++c.factory_calls; });
    wire(pool, c);
    check(c.factory_calls == 2, "factory runs once per pool slot");
    check(pool.size() == 2 && pool.active_count() == 0, "starts all free");
    check(!pool.position_at(0).has_value(), "free slot has no position");

    std::vector<Vector3Type> desired = {{100.f, 0.f, 0.f},
                                        {30.f, 0.f, 0.f},
                                        {2.f, 0.f, 0.f},
                                        {1.f, 0.f, 0.f}};
    pool.update({0.f, 0.f, 0.f}, desired);
    check(pool.active_count() == 2, "two nearest cover the pool");
    check(c.acquires == 2 && c.releases == 0, "two acquires, no releases");
    bool got1 = false, got2 = false;
    for (auto &p : c.acquire_positions) {
      if (at(p, 1.f, 0.f, 0.f))
        got1 = true;
      if (at(p, 2.f, 0.f, 0.f))
        got2 = true;
    }
    check(got1 && got2, "acquires carry the two nearest positions");
    check(c.acquire_indices.size() == 2, "indices ride along");
  }
  {
    EntityCollection col;
    Counters c;
    ProximityPool pool(col, 3);
    wire(pool, c);
    std::vector<Vector3Type> desired = {{5.f, 0.f, 0.f},
                                        {8.f, 0.f, 0.f},
                                        {12.f, 0.f, 0.f},
                                        {40.f, 0.f, 0.f}};
    pool.update({0.f, 0.f, 0.f}, desired);
    check(pool.active_count() == 3, "three nearest active");
    OptEntity e5;
    for (size_t s = 0; s < pool.size(); ++s) {
      auto p = pool.position_at(s);
      if (p && at(*p, 5.f, 0.f, 0.f))
        e5 = pool.entity_at(s);
    }
    check(e5.valid(), "entity on the nearest position exists");
    c.reset();
    pool.update({1.f, 0.f, 0.f}, desired);
    check(c.acquires == 0 && c.releases == 0,
          "unchanged nearest set churns nothing");
    bool kept = false;
    for (size_t s = 0; s < pool.size(); ++s) {
      auto p = pool.position_at(s);
      if (p && at(*p, 5.f, 0.f, 0.f) &&
          pool.entity_at(s).value() == e5.value())
        kept = true;
    }
    check(kept, "nearest position keeps its entity");
  }
  {
    EntityCollection col;
    Counters c;
    ProximityPool pool(col, 2);
    wire(pool, c);
    std::vector<Vector3Type> desired = {{1.f, 0.f, 0.f},
                                        {2.f, 0.f, 0.f},
                                        {100.f, 0.f, 0.f}};
    pool.update({0.f, 0.f, 0.f}, desired);
    check(pool.active_count() == 2, "two active over three desired");
    OptEntity e1, e2;
    for (size_t s = 0; s < pool.size(); ++s) {
      auto p = pool.position_at(s);
      if (p && at(*p, 1.f, 0.f, 0.f))
        e1 = pool.entity_at(s);
      if (p && at(*p, 2.f, 0.f, 0.f))
        e2 = pool.entity_at(s);
    }
    check(e1.valid() && e2.valid(), "both near slots captured");
    c.reset();

    pool.update({100.f, 0.f, 0.f}, desired);
    check(c.releases == 1 && c.acquires == 1,
          "one recycle: one release, one acquire");
    bool still_kept = false, new_near = false, far_recycled = false;
    for (size_t s = 0; s < pool.size(); ++s) {
      auto p = pool.position_at(s);
      if (!p)
        continue;
      if (at(*p, 100.f, 0.f, 0.f)) {
        new_near = true;
        if (pool.entity_at(s).value() == e1.value())
          far_recycled = true;
      }
      if (at(*p, 2.f, 0.f, 0.f) && pool.entity_at(s).value() == e2.value())
        still_kept = true;
    }
    check(still_kept, "still-nearest position keeps its entity");
    check(new_near, "teleported player gets coverage at the new spot");
    check(far_recycled, "recycled entity came from the farthest slot");
  }
  {
    EntityCollection col;
    Counters c;
    ProximityPool pool(col, 4);
    wire(pool, c);
    std::vector<Vector3Type> desired = {{1.f, 0.f, 0.f},
                                        {3.f, 0.f, 0.f},
                                        {6.f, 0.f, 0.f},
                                        {9.f, 0.f, 0.f}};
    pool.update({0.f, 0.f, 0.f}, desired);
    check(pool.active_count() == 4, "all four covered");
    c.reset();

    desired.resize(1);
    pool.update({0.f, 0.f, 0.f}, desired);
    check(pool.active_count() == 1, "demand drop shrinks to one");
    check(c.releases == 3 && c.acquires == 0, "three releases, no acquires");
    bool only_nearest = false;
    for (size_t s = 0; s < pool.size(); ++s) {
      auto q = pool.position_at(s);
      if (q && at(*q, 1.f, 0.f, 0.f))
        only_nearest = true;
    }
    check(only_nearest && !pool.position_at(1).has_value(),
          "sole survivor sits on the nearest position");
    c.reset();

    std::vector<Vector3Type> none;
    pool.update({0.f, 0.f, 0.f}, none);
    check(pool.active_count() == 0 && c.releases == 1,
          "empty demand releases the last slot");
    check(!pool.entity_at(99).valid() && !pool.position_at(99).has_value(),
          "out-of-range slots read empty");
  }
  {
    EntityCollection col;
    Counters c;
    ProximityPool pool(col, 3);
    wire(pool, c);
    std::vector<Vector3Type> desired;
    for (int i = 0; i < 10; ++i)
      desired.push_back({static_cast<float>(i), 0.f, 0.f});
    pool.update({0.f, 0.f, 0.f}, desired);
    check(pool.active_count() == 3 && c.acquires == 3,
          "capacity caps the active set");
    bool ok = true;
    for (size_t s = 0; s < pool.size(); ++s) {
      auto p = pool.position_at(s);
      if (!p || !(p->x < 3.f))
        ok = false;
    }
    check(ok, "only the three nearest positions are covered");
  }

  std::printf("proximity_pool_test: %d/%d checks passed\n", checks - failures,
              checks);
  return failures > 0 ? 1 : 0;
}
