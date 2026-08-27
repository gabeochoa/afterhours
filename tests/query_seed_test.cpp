// whereID used to compare every entity's id against the target, even though
// the collection answers that in O(1) through id_to_slot. It now seeds the
// query's candidate set instead of scanning.
//
// The seed replaces WHAT the query walks, not the filter pipeline, so these
// tests care about one thing above all: the answer must not change. Every
// terminal is covered, because the query has four separate loops over the
// entity vector and wiring only one of them would still pass a test that
// called gen().

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>

#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace afterhours;

// Counting real allocations, because the question "does this cost memory when
// you do not use it" is not answerable by reading the code: add_mod already
// makes a shared_ptr and a std::function per filter, so the seed's own
// allocation has to be measured against that, not against zero.
static std::size_t g_allocs = 0;
static std::size_t g_bytes = 0;
void *operator new(std::size_t n) {
  g_allocs++;
  g_bytes += n;
  return std::malloc(n);
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }

struct AllocScope {
  std::size_t a0, b0;
  AllocScope() : a0(g_allocs), b0(g_bytes) {}
  std::size_t allocs() const { return g_allocs - a0; }
  std::size_t bytes() const { return g_bytes - b0; }
};

static int checks = 0;
static int failures = 0;
static void check(bool cond, const char *expr, int line) {
  checks++;
  if (!cond) {
    failures++;
    std::fprintf(stderr, "  FAIL: %s  (line %d)\n", expr, line);
  }
}
#define CHECK(expr) check((expr), #expr, __LINE__)

struct Tag : BaseComponent {};
struct Other : BaseComponent {};

static EntityCollection make(int count) {
  EntityCollection ec;
  for (int i = 0; i < count; ++i)
    ec.createEntity().addComponent<Tag>();
  ec.cleanup();
  return ec;
}

int main() {
  // The basics: the seeded answer is the scan's answer.
  {
    EntityCollection ec = make(20);
    const EntityID target = ec.get_entities()[7]->id;

    auto got = EntityQuery(ec).whereID(target).gen();
    CHECK(got.size() == 1);
    CHECK(!got.empty() && got[0].get().id == target);
  }

  // A missing id finds nothing rather than the first entity, or a crash.
  {
    EntityCollection ec = make(5);
    CHECK(EntityQuery(ec).whereID(999999).gen().empty());
    CHECK(!EntityQuery(ec).whereID(999999).gen_first().has_value());
    CHECK(EntityQuery(ec).whereID(-1).gen().empty());
  }

  // Every terminal, because each walks its own loop.
  {
    EntityCollection ec = make(30);
    const EntityID target = ec.get_entities()[11]->id;

    CHECK(EntityQuery(ec).whereID(target).gen().size() == 1);
    CHECK(EntityQuery(ec).whereID(target).gen_first().has_value());
    CHECK(EntityQuery(ec).whereID(target).gen_count() == 1);

    // gen_extreme_by has its own scan and ignores run_query entirely.
    auto mn = EntityQuery(ec).whereID(target).gen_min_by(
        [](const Entity &e) { return (float)e.id; });
    CHECK(mn.has_value() && mn.asE().id == target);

    // for_each_stream has a fourth loop of its own.
    int seen = 0;
    EntityQuery(ec).whereID(target).for_each_stream(
        [&](const Entity &e) { seen += (e.id == target) ? 1 : -100; });
    CHECK(seen == 1);
  }

  // The seed narrows the walk; it does not bypass the other filters.
  {
    EntityCollection ec;
    Entity &a = ec.createEntity();
    a.addComponent<Tag>();
    Entity &b = ec.createEntity();
    b.addComponent<Other>();
    ec.cleanup();

    // b has no Tag, so asking for b AND Tag must find nothing.
    CHECK(EntityQuery(ec).whereID(b.id).whereHasComponent<Tag>().gen().empty());
    CHECK(EntityQuery(ec).whereID(a.id).whereHasComponent<Tag>().gen().size() ==
          1);
    // Order of the two calls must not matter.
    CHECK(EntityQuery(ec).whereHasComponent<Tag>().whereID(a.id).gen().size() ==
          1);
  }

  // whereNotID is a negation and cannot seed: it must still scan and return
  // everything else.
  {
    EntityCollection ec = make(6);
    const EntityID target = ec.get_entities()[2]->id;
    CHECK(EntityQuery(ec).whereNotID(target).gen().size() == 5);
  }

  // The shortcut reads the slot table, which sees temp entities that the scan
  // does not. When temp is non-empty the two disagree, so the shortcut must
  // stand down rather than report an entity the query would never have found.
  {
    EntityCollection ec = make(4);
    Entity &pending = ec.createEntity(); // temp, not merged
    pending.addComponent<Tag>();

    // A query built now must not see the temp entity, seeded or not.
    CHECK(EntityQuery(ec, {.ignore_temp_warning = true})
              .whereID(pending.id)
              .gen()
              .empty());

    // After the merge it is a normal entity and the shortcut applies.
    ec.merge_entity_arrays();
    CHECK(EntityQuery(ec).whereID(pending.id).gen().size() == 1);
  }

  // A query built from a bare vector has no collection to ask, so it falls
  // back to the scan and still answers correctly.
  {
    EntityCollection ec = make(8);
    const Entities &raw = ec.get_entities();
    const EntityID target = raw[3]->id;
    CHECK(EntityQuery(raw).whereID(target).gen().size() == 1);
  }

  // The point of the shortcut: the work stops scaling with the collection.
  // Counted, not timed, so it is deterministic and says WHY it is faster.
  {
    std::size_t evals_1k = 0, evals_2k = 0;
    for (int n : {1000, 2000}) {
      EntityCollection ec = make(n);
      const EntityID target = ec.get_entities()[n / 2]->id;
      std::size_t evals = 0;
      auto r = EntityQuery(ec)
                   .whereID(target)
                   .whereLambda([&](const Entity &) {
                     evals++;
                     return true;
                   })
                   .gen();
      CHECK(r.size() == 1);
      (n == 1000 ? evals_1k : evals_2k) = evals;
    }
    std::printf("  filter evals after whereID: %zu at 1k, %zu at 2k\n",
                evals_1k, evals_2k);
    // Doubling the collection must not change the work. Before the shortcut
    // this was 1000 then 2000.
    CHECK(evals_1k == evals_2k);
    CHECK(evals_1k <= 1);

    // The unindexed spelling is what we are measuring against: it does scale.
    std::size_t scan_1k = 0, scan_2k = 0;
    for (int n : {1000, 2000}) {
      EntityCollection ec = make(n);
      const EntityID target = ec.get_entities()[n / 2]->id;
      std::size_t evals = 0;
      auto r = EntityQuery(ec)
                   .whereLambda([&](const Entity &e) {
                     evals++;
                     return e.id == target;
                   })
                   .gen();
      CHECK(r.size() == 1);
      (n == 1000 ? scan_1k : scan_2k) = evals;
    }
    std::printf("  filter evals scanning:      %zu at 1k, %zu at 2k\n", scan_1k,
                scan_2k);
    CHECK(scan_2k >= scan_1k * 2);
  }

  // Memory. The seed is an optional<Entities>, which allocates nothing until
  // something is put in it, so a query that never seeds must not pay for the
  // feature existing.
  {
    EntityCollection ec = make(1000);
    const EntityID target = ec.get_entities()[500]->id;

    std::size_t unseeded = 0, seeded = 0;
    {
      AllocScope s;
      auto r = EntityQuery(ec).whereHasComponent<Tag>().gen();
      (void)r;
      unseeded = s.allocs();
    }
    {
      AllocScope s;
      auto r = EntityQuery(ec).whereID(target).gen();
      (void)r;
      seeded = s.allocs();
    }
    std::printf("  allocs: unseeded query %zu, seeded whereID %zu\n", unseeded,
                seeded);
    // The seed costs one vector allocation on top of whatever the filter
    // machinery already spends. If this ever grows, the shortcut stopped being
    // a shortcut.
    CHECK(seeded <= unseeded + 2);

    // sizeof is stack, not heap, but it is paid by every query in the process
    // whether or not it seeds, so it is worth a ceiling.
    std::printf("  sizeof(EntityQuery<>) = %zu\n", sizeof(EntityQuery<>));
    CHECK(sizeof(EntityQuery<>) <= 200);
  }

  std::printf("%d/%d checks passed\n", checks - failures, checks);
  if (failures == 0) std::printf("All checks passed!\n");
  return failures == 0 ? 0 : 1;
}
