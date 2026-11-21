// entity_membership_test.cpp
// Per-component membership lists (gap #20): component queries and system
// passes walk the smallest membership list instead of every entity.
//
// Two claims, both counted/compared, never timed:
//   1. Equivalence: every result below is the set AND order a full scan of
//      the entity vector produces, stated explicitly at each site.
//   2. Scaling: entities visited (a filter counter placed BEFORE the
//      component filter, so it counts the walk, not the matches) stay put
//      when the world doubles and the answer does not.

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>

#include <cstdio>
#include <vector>

using namespace afterhours;

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

struct Pos : BaseComponent { int x = 0; };
struct Vel : BaseComponent { int x = 0; };
struct Rare : BaseComponent {};
struct Solo : BaseComponent {};

static std::vector<EntityID> ids_of(const RefEntities &ents) {
  std::vector<EntityID> out;
  for (const Entity &e : ents) out.push_back(e.id);
  return out;
}

template <typename... Cs> struct Recorder : System<Cs...> {
  std::vector<EntityID> seen;
  void for_each_with(Entity &e, Cs &..., const float) override {
    seen.push_back(e.id);
  }
};

// Removes Rare from the NEXT entity (by creation list) the first time it
// runs, to pin mid-pass removal semantics.
struct Remover : System<Rare> {
  std::vector<EntityID> seen;
  Entity *victim = nullptr;
  void for_each_with(Entity &e, Rare &, const float) override {
    seen.push_back(e.id);
    if (victim) {
      victim->removeComponent<Rare>();
      victim = nullptr;
    }
  }
};

// Adds Rare to a bystander the first time it runs, to pin mid-pass
// addition semantics.
struct Adder : System<Rare> {
  std::vector<EntityID> seen;
  Entity *bystander = nullptr;
  void for_each_with(Entity &e, Rare &, const float) override {
    seen.push_back(e.id);
    if (bystander) {
      bystander->addComponent<Rare>();
      bystander = nullptr;
    }
  }
};

int main() {
  // --- equivalence: interleaved component sets, explicit order ----------
  {
    EntityCollection ec;
    std::vector<EntityID> pv, p_only, all;
    for (int i = 0; i < 6; i++) {
      Entity &e = ec.createEntity();
      all.push_back(e.id);
      if (i % 2 == 0) e.addComponent<Pos>();
      if (i % 3 == 0) { e.addComponent<Vel>(); }
      if (i % 2 == 0 && i % 3 == 0) pv.push_back(e.id);
      else if (i % 2 == 0) p_only.push_back(e.id);
    }
    ec.cleanup();

    CHECK(ids_of(EntityQuery(ec).whereHasComponent<Pos>().gen()) ==
          std::vector<EntityID>({all[0], all[2], all[4]}));
    CHECK(ids_of(EntityQuery(ec).whereHasComponent<Pos, Vel>().gen()) == pv);
    CHECK(ids_of(EntityQuery(ec).whereHasComponent<Vel, Pos>().gen()) == pv);
    CHECK(ids_of(EntityQuery(ec).whereMissingComponent<Pos>().gen()) ==
          std::vector<EntityID>({all[1], all[3], all[5]}));

    // Every terminal walks pool() its own way; cover each.
    CHECK(EntityQuery(ec).whereHasComponent<Pos>().gen_count() == 3);
    auto first = EntityQuery(ec).whereHasComponent<Pos>().gen_first();
    CHECK(first.has_value() && first.asE().id == all[0]);
    std::vector<EntityID> streamed;
    EntityQuery(ec).whereHasComponent<Pos>().for_each_stream(
        [&](const Entity &e) { streamed.push_back(e.id); });
    CHECK(streamed == std::vector<EntityID>({all[0], all[2], all[4]}));
    auto mn = EntityQuery(ec).whereHasComponent<Pos>().gen_min_by(
        [](const Entity &e) { return static_cast<float>(e.id); });
    CHECK(mn.has_value() && mn.asE().id == all[0]);
    CHECK(EntityQuery(ec)
              .whereComponentAndLambda<Pos>(
                  [](const Pos &p) { return p.x >= 0; })
              .gen()
              .size() == 3);
  }

  // --- order: a component added late keeps the entity's vector slot -----
  {
    EntityCollection ec;
    Entity &early = ec.createEntity();
    const EntityID early_id = early.id;
    std::vector<EntityID> expected;
    for (int i = 0; i < 5; i++) {
      Entity &e = ec.createEntity();
      e.addComponent<Pos>();
      expected.push_back(e.id);
    }
    ec.cleanup();
    // `early` sits at vector position 0 but gains Pos last. A membership
    // list in add-order would return it last; the scan returns it first.
    early.addComponent<Pos>();
    expected.insert(expected.begin(), early_id);
    CHECK(ids_of(EntityQuery(ec).whereHasComponent<Pos>().gen()) == expected);
  }

  // --- churn: remove, retire, cleanup-flag visibility -------------------
  {
    EntityCollection ec;
    Entity &a = ec.createEntity(); a.addComponent<Pos>();
    Entity &b = ec.createEntity(); b.addComponent<Pos>();
    Entity &c = ec.createEntity(); c.addComponent<Pos>();
    const EntityID aid = a.id, bid = b.id, cid = c.id;
    ec.cleanup();
    CHECK(ids_of(EntityQuery(ec).whereHasComponent<Pos>().gen()) ==
          std::vector<EntityID>({aid, bid, cid}));

    b.removeComponent<Pos>();
    CHECK(ids_of(EntityQuery(ec).whereHasComponent<Pos>().gen()) ==
          std::vector<EntityID>({aid, cid}));

    // Flagged for cleanup but not yet collected: the scan still sees it,
    // so the membership walk must too.
    ec.markIDForCleanup(cid);
    CHECK(ids_of(EntityQuery(ec).whereHasComponent<Pos>().gen()) ==
          std::vector<EntityID>({aid, cid}));
    CHECK(ids_of(EntityQuery(ec)
                     .whereHasComponent<Pos>()
                     .whereNotMarkedForCleanup()
                     .gen()) == std::vector<EntityID>({aid}));

    ec.cleanup();
    CHECK(ids_of(EntityQuery(ec).whereHasComponent<Pos>().gen()) ==
          std::vector<EntityID>({aid}));

    // Recycled slot/pool reuse must not resurrect the retired entity.
    Entity &d = ec.createEntity(); d.addComponent<Pos>();
    const EntityID did = d.id;
    ec.cleanup();
    CHECK(ids_of(EntityQuery(ec).whereHasComponent<Pos>().gen()) ==
          std::vector<EntityID>({aid, did}));
  }

  // --- temp entities: unseen before merge, seen after -------------------
  {
    EntityCollection ec;
    ec.createEntity().addComponent<Pos>();
    ec.cleanup();
    Entity &pending = ec.createEntity();
    pending.addComponent<Pos>();
    const EntityID pending_id = pending.id;
    CHECK(EntityQuery(ec, {.ignore_temp_warning = true})
              .whereHasComponent<Pos>()
              .gen()
              .size() == 1);
    ec.merge_entity_arrays();
    auto got = EntityQuery(ec).whereHasComponent<Pos>().gen();
    CHECK(got.size() == 2 && got[1].get().id == pending_id);
  }

  // --- singleton: direct fetch and singleton-component query ------------
  {
    EntityCollection ec;
    Entity &host = ec.createEntity();
    host.addComponent<Solo>();
    const EntityID host_id = host.id;
    for (int i = 0; i < 20; i++) ec.createEntity().addComponent<Pos>();
    ec.cleanup();
    ec.registerSingleton<Solo>(host);
    CHECK(ec.get_singleton_cmp<Solo>() != nullptr);
    CHECK(ec.get_singleton_cmp<Solo>() == &host.get<Solo>());
    CHECK(ids_of(EntityQuery(ec).whereHasComponent<Solo>().gen()) ==
          std::vector<EntityID>({host_id}));
  }

  // --- system pass: only holders, in entity order; mid-pass edges -------
  {
    EntityCollection ec;
    auto &entities = ec.get_entities_for_mod();
    std::vector<EntityID> expected;
    for (int i = 0; i < 10; i++) {
      Entity &e = ec.createEntity();
      if (i % 3 == 0) { e.addComponent<Rare>(); expected.push_back(e.id); }
      else e.addComponent<Pos>();
    }
    ec.cleanup();
    Recorder<Rare> sys;
    run_system_over(sys, entities, 0.f, ec, false);
    CHECK(sys.seen == expected);

    // Removed mid-pass: for_each re-checks, so the victim is skipped.
    Remover rem;
    auto &ents = ec.get_entities();
    rem.victim = ents[3].get();  // second Rare holder (i=3)
    run_system_over(rem, entities, 0.f, ec, false);
    CHECK(rem.seen == std::vector<EntityID>({expected[0], expected[2],
                                             expected[3]}));

    // Added mid-pass: not in the pass-start snapshot, visited next pass.
    Adder add;
    Entity &bystander = *ents[1];  // Pos-only entity
    const EntityID bystander_id = bystander.id;
    add.bystander = &bystander;
    run_system_over(add, entities, 0.f, ec, false);
    CHECK(add.seen == std::vector<EntityID>({expected[0], expected[2],
                                             expected[3]}));
    Recorder<Rare> again;
    run_system_over(again, entities, 0.f, ec, false);
    CHECK(again.seen == std::vector<EntityID>(
                            {expected[0], bystander_id, expected[2],
                             expected[3]}));
  }

  // --- scaling proof: visits track matches, not world size --------------
  {
    auto visits = [](int world, int matches, std::size_t &rebuilds) {
      EntityCollection ec;
      for (int i = 0; i < world; i++) {
        Entity &e = ec.createEntity();
        e.addComponent<Pos>();
        if (i < matches) e.addComponent<Rare>();
      }
      ec.cleanup();
      const std::size_t r0 = EntityCollection::stats().membership_rebuilds;
      std::size_t visited = 0;
      auto r = EntityQuery(ec)
                   .whereLambda([&](const Entity &) {
                     visited++;
                     return true;
                   })
                   .whereHasComponent<Rare>()
                   .gen();
      rebuilds = EntityCollection::stats().membership_rebuilds - r0;
      CHECK(r.size() == static_cast<std::size_t>(matches));
      // Second identical query: no rebuild, same visit count.
      std::size_t visited2 = 0;
      auto r2 = EntityQuery(ec)
          .whereLambda([&](const Entity &) {
            visited2++;
            return true;
          })
          .whereHasComponent<Rare>()
          .gen();
      (void)r2;
      CHECK(visited2 == visited);
      CHECK(EntityCollection::stats().membership_rebuilds - r0 == rebuilds);
      return visited;
    };
    std::size_t rb_small = 0, rb_big = 0;
    const std::size_t v_small = visits(2000, 10, rb_small);
    const std::size_t v_big = visits(4000, 10, rb_big);
    std::printf("  visits: %zu at 2k world, %zu at 4k world (10 matches)\n",
                v_small, v_big);
    CHECK(v_small == 10 && v_big == 10);
    CHECK(rb_small == 1 && rb_big == 1);

    // Multi-component: walk starts from the smallest list (Rare=10), not
    // the world and not the common component (Pos=world).
    EntityCollection ec;
    for (int i = 0; i < 3000; i++) {
      Entity &e = ec.createEntity();
      e.addComponent<Pos>();
      if (i % 300 == 0) e.addComponent<Rare>();
    }
    ec.cleanup();
    std::size_t visited = 0;
    auto r = EntityQuery(ec)
                 .whereLambda([&](const Entity &) {
                   visited++;
                   return true;
                 })
                 .whereHasComponent<Pos, Rare>()
                 .gen();
    CHECK(r.size() == 10);
    CHECK(visited == 10);
  }

  std::printf("%d/%d checks passed\n", checks - failures, checks);
  if (failures == 0) std::printf("All checks passed!\n");
  return failures == 0 ? 0 : 1;
}
