// Does the index earn its place?
//
// Counted, never timed. Wall clock depends on the machine and on whether
// somebody remembered -O2, and this project has already been burned once by a
// number that turned out to be a missing optimisation flag. Counts are
// deterministic and they say WHY something is faster.
//
// Both sides are counted. The win is easy to show and proves nothing on its
// own; what matters is whether it beats the overhead this design added, which
// is a handle resolve per bucket row plus a full pass whenever the collection
// changes.

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>

#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace afterhours;

static std::size_t g_allocs = 0;
static std::size_t g_bytes = 0;
static bool g_counting = false;
void *operator new(std::size_t n) {
  if (g_counting) {
    g_allocs++;
    g_bytes += n;
  }
  return std::malloc(n);
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }

static int checks = 0;
static int failures = 0;
static void check(bool cond, const char *expr, int line) {
  checks++;
  if (!cond) {
    failures++;
    std::fprintf(stderr, "  FAIL: %s  (line %d)\n", expr, line);
  }
}
#define CHECK(...) check((__VA_ARGS__), #__VA_ARGS__, __LINE__)

struct Child : BaseComponent {
  EntityHandle parent{EntityHandle::invalid()};
};
struct Tint : BaseComponent {
  int shade = 0;
};
struct Mass : BaseComponent {
  int kg = 0;
};

// One parent with `bucket` children, padded out to `total` entities so the
// collection can grow while the answer stays the same size.
struct Fixture {
  EntityCollection ec;
  EntityHandle parent{EntityHandle::invalid()};

  Fixture(int total, int bucket) {
    Entity &p = ec.createEntity();
    ec.cleanup();
    parent = ec.handle_for(p);
    for (int i = 0; i < bucket; i++)
      ec.createEntity().addComponent<Child>().parent = parent;
    for (int i = 0; i < total - bucket - 1; i++) {
      Entity &e = ec.createEntity();
      e.addComponent<Tint>().shade = i % 8;
      e.addComponent<Mass>().kg = i;
    }
    ec.cleanup();
    ec.add_index<Child>([](const Child &c) { return c.parent; });
    ec.ensure_indexes_fresh();
  }
};

// Entities VISITED, which is the cost. The counter has to come before
// whereHasComponent: put it after and the component test short-circuits first,
// so it reports the handful that matched and hides the whole walk. Measured
// that mistake before fixing it.
static std::size_t scan_evals(EntityCollection &ec, EntityHandle parent) {
  std::size_t visited = 0;
  auto r = EntityQuery(ec)
               .whereLambda([&](const Entity &) {
                 visited++;
                 return true;
               })
               .whereHasComponent<Child>()
               .whereLambda([&](const Entity &e) {
                 return e.get<Child>().parent == parent;
               })
               .gen();
  (void)r;
  return visited;
}

static std::size_t indexed_resolves(EntityCollection &ec, EntityHandle parent) {
  const std::size_t before = EntityCollection::stats().bucket_resolves;
  auto r = EntityQuery(ec).whereIndexed<Child>(parent).gen();
  (void)r;
  return EntityCollection::stats().bucket_resolves - before;
}

int main() {
  const int BUCKET = 10;

  // 1. Double the collection, hold the answer size. The scan doubles; the
  //    indexed walk does not move. This is the whole claim.
  std::size_t scan1k = 0, scan2k = 0, idx1k = 0, idx2k = 0;
  {
    Fixture a(1000, BUCKET);
    Fixture b(2000, BUCKET);
    CHECK(EntityQuery(a.ec).whereIndexed<Child>(a.parent).gen().size() ==
          (std::size_t)BUCKET);
    CHECK(EntityQuery(b.ec).whereIndexed<Child>(b.parent).gen().size() ==
          (std::size_t)BUCKET);

    scan1k = scan_evals(a.ec, a.parent);
    scan2k = scan_evals(b.ec, b.parent);
    idx1k = indexed_resolves(a.ec, a.parent);
    idx2k = indexed_resolves(b.ec, b.parent);

    std::printf("  scan evals:      %zu at 1k, %zu at 2k\n", scan1k, scan2k);
    std::printf("  bucket resolves: %zu at 1k, %zu at 2k\n", idx1k, idx2k);
    CHECK(scan2k >= scan1k * 2);
    CHECK(idx1k == idx2k);
    CHECK(idx1k == (std::size_t)BUCKET);
  }

  // 2. One rebuild serves every lookup at the same version.
  {
    Fixture f(1000, BUCKET);
    const std::size_t r0 = EntityCollection::stats().index_rebuilds;
    for (int i = 0; i < 100; i++) {
      auto r = EntityQuery(f.ec).whereIndexed<Child>(f.parent).gen();
      (void)r;
    }
    CHECK(EntityCollection::stats().index_rebuilds == r0);
  }

  // 3. A second and third index cost no extra pass over the entities.
  std::size_t considers_1, considers_3;
  {
    Fixture f(1000, BUCKET);
    f.ec.invalidate_indexes();
    std::size_t c0 = EntityCollection::stats().index_considers;
    f.ec.ensure_indexes_fresh();
    considers_1 = EntityCollection::stats().index_considers - c0;

    f.ec.add_index<Tint>([](const Tint &t) { return t.shade; });
    f.ec.add_index<Mass>([](const Mass &m) { return m.kg; });
    c0 = EntityCollection::stats().index_considers;
    f.ec.ensure_indexes_fresh();
    considers_3 = EntityCollection::stats().index_considers - c0;

    std::printf("  considers: %zu with 1 index, %zu with 3\n", considers_1,
                considers_3);
    // Three indexes means three times the per-entity work, but still ONE walk
    // of the entity vector. The hand-rolled version pays three walks.
    CHECK(considers_3 == considers_1 * 3);
    const std::size_t rb = EntityCollection::stats().index_rebuilds;
    f.ec.ensure_indexes_fresh();
    CHECK(EntityCollection::stats().index_rebuilds == rb);
  }

  // 4. Memory. Nothing registered must cost nothing, and what a registered
  //    index holds should scale with the entities it indexes, not the
  //    collection.
  {
    EntityCollection bare;
    for (int i = 0; i < 2000; i++)
      bare.createEntity().addComponent<Tint>().shade = i;
    bare.cleanup();
    g_counting = true;
    const std::size_t a0 = g_allocs, b0 = g_bytes;
    for (int i = 0; i < 100; i++)
      bare.ensure_indexes_fresh();
    const std::size_t bare_allocs = g_allocs - a0, bare_bytes = g_bytes - b0;
    g_counting = false;
    std::printf("  no index registered: %zu allocs, %zu bytes over 100 frames\n",
                bare_allocs, bare_bytes);
    CHECK(bare_allocs == 0);
    CHECK(bare_bytes == 0);

    for (int total : {1000, 2000}) {
      Fixture f(total, BUCKET);
      f.ec.invalidate_indexes();
      g_counting = true;
      const std::size_t ab = g_allocs, bb = g_bytes;
      f.ec.ensure_indexes_fresh();
      const std::size_t used = g_bytes - bb, na = g_allocs - ab;
      g_counting = false;
      std::printf("  index over %d entities (%d indexed): %zu bytes in %zu allocs\n",
                  total, BUCKET, used, na);
      // Only the children are stored, so the cost tracks the bucket and not
      // the collection. Generous ceiling; the point is that it is not O(total).
      CHECK(used < 4096);
    }
  }

  // 5. Every terminal narrows, not just gen(). A partial task 3 shows up here
  //    as an unchanged scan count.
  {
    Fixture f(2000, BUCKET);
    const auto measure = [&](const char *what, auto &&run) {
      const std::size_t b = EntityCollection::stats().bucket_resolves;
      run();
      const std::size_t used = EntityCollection::stats().bucket_resolves - b;
      std::printf("  %-16s resolves %zu (collection is 2000)\n", what, used);
      check(used == (std::size_t)BUCKET, what, __LINE__);
    };
    measure("gen", [&] {
      auto r = EntityQuery(f.ec).whereIndexed<Child>(f.parent).gen();
      (void)r;
    });
    measure("gen_first", [&] {
      auto r = EntityQuery(f.ec).whereIndexed<Child>(f.parent).gen_first();
      (void)r;
    });
    measure("gen_min_by", [&] {
      auto r = EntityQuery(f.ec).whereIndexed<Child>(f.parent).gen_min_by(
          [](const Entity &e) { return (float)e.id; });
      (void)r;
    });
    measure("for_each_stream", [&] {
      EntityQuery(f.ec).whereIndexed<Child>(f.parent).for_each_stream(
          [](const Entity &) {});
    });
  }

  // 6. The crossover: how many lookups before the index has paid for itself.
  //    If this were large enough that no real frame reaches it, the honest
  //    answer would be not to ship the index at all.
  {
    const std::size_t E = 1000;
    Fixture f((int)E, BUCKET);
    f.ec.invalidate_indexes();
    const std::size_t c0 = EntityCollection::stats().index_considers;
    f.ec.ensure_indexes_fresh();
    const std::size_t rebuild_cost = EntityCollection::stats().index_considers - c0;

    const std::size_t scan_per_lookup = scan_evals(f.ec, f.parent);
    const std::size_t idx_per_lookup = indexed_resolves(f.ec, f.parent) * 2;

    std::size_t crossover = 0;
    for (std::size_t L = 1; L <= 10000; L++) {
      if (L * scan_per_lookup < rebuild_cost + L * idx_per_lookup)
        continue;
      crossover = L;
      break;
    }
    std::printf("\n  CROSSOVER at %zu entities, %d indexed:\n", E, BUCKET);
    std::printf("    rebuild costs %zu, scan %zu per lookup, index %zu per lookup\n",
                rebuild_cost, scan_per_lookup, idx_per_lookup);
    std::printf("    the index pays for itself after %zu lookup(s) per rebuild\n\n",
                crossover);
    // A frame that looks something up once and never again would not justify
    // this. Anything that asks twice does.
    CHECK(crossover > 0);
    CHECK(crossover <= 10);
  }

  std::printf("%d/%d checks passed\n", checks - failures, checks);
  if (failures == 0) std::printf("All checks passed!\n");
  return failures == 0 ? 0 : 1;
}
