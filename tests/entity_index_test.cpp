// A secondary index over a component field, rebuilt from the entity vector
// rather than maintained.
//
// The rebuild-not-maintain choice is the whole design, so most of what is
// checked here is the invalidation: an index that answers from a version that
// has moved is worse than no index. The handle-vs-shared_ptr choice gets its
// own case, because a shared_ptr bucket would pass every other test in this
// file while handing back a deleted entity.

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>

#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace afterhours;

static std::size_t g_allocs = 0;
static std::size_t g_bytes = 0;
void *operator new(std::size_t n) {
  g_allocs++;
  g_bytes += n;
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
// Variadic so a template argument list with a comma survives.
#define CHECK(...) check((__VA_ARGS__), #__VA_ARGS__, __LINE__)

// A child points at its parent by handle, which is puzzle's exact shape.
struct Child : BaseComponent {
  EntityHandle parent{EntityHandle::invalid()};
};
struct Other : BaseComponent {
  int group = 0;
};
struct Unrelated : BaseComponent {};

static void index_children(EntityCollection &ec) {
  ec.add_index<Child>([](const Child &c) { return c.parent; });
}

int main() {
  // The basic shape: children group under their parent.
  {
    EntityCollection ec;
    Entity &p1 = ec.createEntity();
    Entity &p2 = ec.createEntity();
    ec.cleanup();
    const EntityHandle h1 = ec.handle_for(p1);
    const EntityHandle h2 = ec.handle_for(p2);

    for (int i = 0; i < 3; i++)
      ec.createEntity().addComponent<Child>().parent = h1;
    ec.createEntity().addComponent<Child>().parent = h2;
    ec.cleanup();

    index_children(ec);
    CHECK(ec.indexed<Child>(h1).size() == 3);
    CHECK(ec.indexed<Child>(h2).size() == 1);
    CHECK(ec.indexed<Child>(EntityHandle::invalid()).empty());
  }

  // Entities added after the index was built must show up. This is the
  // version gate doing its job.
  {
    EntityCollection ec;
    Entity &p = ec.createEntity();
    ec.cleanup();
    const EntityHandle h = ec.handle_for(p);
    index_children(ec);
    CHECK(ec.indexed<Child>(h).empty());

    ec.createEntity().addComponent<Child>().parent = h;
    ec.cleanup();
    CHECK(ec.indexed<Child>(h).size() == 1);

    ec.createEntity().addComponent<Child>().parent = h;
    ec.cleanup();
    CHECK(ec.indexed<Child>(h).size() == 2);
  }

  // THE CASE THAT PINS THE DESIGN.
  // Delete an indexed entity and do NOT rebuild, then resolve what the bucket
  // still holds. With shared_ptr buckets the entity would still be there, with
  // every component attached, indistinguishable from a live one. A handle
  // fails its generation check.
  {
    EntityCollection ec;
    Entity &p = ec.createEntity();
    ec.cleanup();
    const EntityHandle h = ec.handle_for(p);
    Entity &doomed = ec.createEntity();
    doomed.addComponent<Child>().parent = h;
    ec.cleanup();

    index_children(ec);
    const auto &before = ec.indexed<Child>(h);
    CHECK(before.size() == 1);
    const EntityHandle child_handle = before[0];
    CHECK(ec.resolve(child_handle).has_value());

    // Kill it, then look at the handle we already took. No query, so no
    // rebuild: this is the stale bucket state on purpose.
    doomed.cleanup = true;
    ec.cleanup();
    CHECK(!ec.resolve(child_handle).has_value());

    // And the next query rebuilds, so the row is gone entirely.
    CHECK(ec.indexed<Child>(h).empty());
  }

  // A key written through get<C>() cannot be seen by the version gate. The
  // index keeps the old answer until someone says otherwise. Documenting the
  // hole rather than pretending it is closed.
  {
    EntityCollection ec;
    Entity &p1 = ec.createEntity();
    Entity &p2 = ec.createEntity();
    ec.cleanup();
    const EntityHandle h1 = ec.handle_for(p1);
    const EntityHandle h2 = ec.handle_for(p2);
    Entity &c = ec.createEntity();
    c.addComponent<Child>().parent = h1;
    ec.cleanup();

    index_children(ec);
    CHECK(ec.indexed<Child>(h1).size() == 1);

    c.get<Child>().parent = h2; // no hook fires, nothing bumps
    CHECK(ec.indexed<Child>(h1).size() == 1); // stale, and that is the point
    CHECK(ec.indexed<Child>(h2).empty());

    ec.invalidate_indexes();
    CHECK(ec.indexed<Child>(h1).empty());
    CHECK(ec.indexed<Child>(h2).size() == 1);
  }

  // One pass feeds every index, however many are registered.
  {
    EntityCollection ec;
    Entity &p = ec.createEntity();
    ec.cleanup();
    const EntityHandle h = ec.handle_for(p);
    for (int i = 0; i < 10; i++) {
      Entity &e = ec.createEntity();
      e.addComponent<Child>().parent = h;
      e.addComponent<Other>().group = i % 2;
    }
    ec.cleanup();

    ec.add_index<Child>([](const Child &c) { return c.parent; });
    ec.add_index<Other>([](const Other &o) { return o.group; });

    const std::size_t before = EntityCollection::stats().index_rebuilds;
    CHECK(ec.indexed<Child>(h).size() == 10);
    CHECK(ec.indexed<Other, int>(0).size() == 5);
    CHECK(ec.indexed<Other, int>(1).size() == 5);
    // Three lookups across two indexes, one pass over the entities.
    CHECK(EntityCollection::stats().index_rebuilds == before + 1);
  }

  // Asking with the wrong key type is a loud error, not a wrong answer.
  {
    EntityCollection ec;
    ec.createEntity().addComponent<Other>().group = 7;
    ec.cleanup();
    ec.add_index<Other>([](const Other &o) { return o.group; });
    CHECK(ec.indexed<Other, int>(7).size() == 1);
    std::printf("  (expect two errors below, they are the point)\n");
    CHECK(ec.indexed<Other, float>(7.f).empty());  // wrong key type
    CHECK(ec.indexed<Unrelated, int>(7).empty());  // never registered
  }

  // Registering nothing must cost nothing: no buckets, no pass, no memory.
  {
    EntityCollection ec;
    for (int i = 0; i < 1000; i++)
      ec.createEntity().addComponent<Other>().group = i;
    ec.cleanup();

    const std::size_t rebuilds = EntityCollection::stats().index_rebuilds;
    const std::size_t considers = EntityCollection::stats().index_considers;
    const std::size_t a0 = g_allocs;
    for (int i = 0; i < 50; i++)
      ec.ensure_indexes_fresh();
    CHECK(EntityCollection::stats().index_rebuilds == rebuilds);
    CHECK(EntityCollection::stats().index_considers == considers);
    CHECK(g_allocs == a0);
    CHECK(ec.indexes.empty());
    CHECK(ec.indexes_are_fresh());
  }

  // whereIndexed must agree with the spelling it replaces, exactly.
  {
    EntityCollection ec;
    Entity &p1 = ec.createEntity();
    Entity &p2 = ec.createEntity();
    ec.cleanup();
    const EntityHandle h1 = ec.handle_for(p1);
    const EntityHandle h2 = ec.handle_for(p2);
    for (int i = 0; i < 7; i++)
      ec.createEntity().addComponent<Child>().parent = (i % 3) ? h1 : h2;
    ec.cleanup();
    index_children(ec);

    const auto scanned = EntityQuery(ec)
                             .whereHasComponent<Child>()
                             .whereLambda([&](const Entity &e) {
                               return e.get<Child>().parent == h1;
                             })
                             .gen();
    const auto seeded = EntityQuery(ec).whereIndexed<Child>(h1).gen();
    CHECK(scanned.size() == seeded.size());
    CHECK(!seeded.empty());

    // Same answer, not just the same count.
    bool same = scanned.size() == seeded.size();
    for (std::size_t i = 0; i < seeded.size() && same; i++) {
      bool found = false;
      for (const auto &sc : scanned)
        if (sc.get().id == seeded[i].get().id) found = true;
      same = found;
    }
    CHECK(same);

    // Every terminal, because the query has four separate loops.
    CHECK(EntityQuery(ec).whereIndexed<Child>(h1).gen_count() == seeded.size());
    CHECK(EntityQuery(ec).whereIndexed<Child>(h1).gen_first().has_value());
    CHECK(EntityQuery(ec)
              .whereIndexed<Child>(h1)
              .gen_min_by([](const Entity &e) { return (float)e.id; })
              .has_value());
    std::size_t streamed = 0;
    EntityQuery(ec).whereIndexed<Child>(h1).for_each_stream(
        [&](const Entity &) { streamed++; });
    CHECK(streamed == seeded.size());
  }

  // Seeding narrows, it does not bypass. A second filter still applies.
  {
    EntityCollection ec;
    Entity &p = ec.createEntity();
    ec.cleanup();
    const EntityHandle h = ec.handle_for(p);
    for (int i = 0; i < 5; i++) {
      Entity &e = ec.createEntity();
      e.addComponent<Child>().parent = h;
      if (i < 2) e.addComponent<Unrelated>();
    }
    ec.cleanup();
    index_children(ec);
    CHECK(EntityQuery(ec).whereIndexed<Child>(h).gen().size() == 5);
    CHECK(EntityQuery(ec)
              .whereIndexed<Child>(h)
              .whereHasComponent<Unrelated>()
              .gen()
              .size() == 2);
    CHECK(EntityQuery(ec).whereIndexed<Child>(h).take(3).gen().size() == 3);
  }

  std::printf("%d/%d checks passed\n", checks - failures, checks);
  if (failures == 0) std::printf("All checks passed!\n");
  return failures == 0 ? 0 : 1;
}
