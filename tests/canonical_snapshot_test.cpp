#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/src/core/snapshot.h>

#include <cstdio>
#include <unordered_map>
#include <string>
#include <vector>

using namespace afterhours;

static int run = 0, passed = 0;
static void check(bool c, const char *e, int line) {
  run++;
  if (c) passed++; else fprintf(stderr, "  FAIL: %s (line %d)\n", e, line);
}
#define CHECK(e) check((e), #e, __LINE__)

struct Fighter { std::string stable_id; int hp; int team; };

struct World {
  snapshot::CanonicalSnapshot snap;
  std::unordered_map<EntityHandle, std::string> ids;
};

static World build(const std::vector<int> &order, std::uint32_t slot_base,
                   int bravo_hp = 20) {
  const std::vector<Fighter> fighters = {{"alpha", 10, 1}, {"bravo", bravo_hp, 1}, {"charlie", 30, 2}};
  World w;
  std::vector<snapshot::EntityRecord> ents;
  std::vector<snapshot::ComponentRecord<Fighter>> comps;
  for (std::size_t k = 0; k < order.size(); k++) {
    const Fighter &f = fighters[(std::size_t)order[k]];
    EntityHandle h{slot_base + (std::uint32_t)k, 1};
    w.ids[h] = f.stable_id;
    ents.push_back(snapshot::EntityRecord{.handle = h, .entity_type = 7, .tags = {}, .cleanup = false});
    comps.push_back(snapshot::ComponentRecord<Fighter>{.entity = h, .value = f});
  }
  auto identity_of = [&](const EntityHandle &h) { return w.ids.at(h); };
  w.snap = snapshot::canonicalize(ents, identity_of);
  snapshot::add_components(w.snap, comps, identity_of, "Fighter",
                           [](const Fighter &f) {
                             return std::vector<snapshot::Field>{
                                 {"hp", std::to_string(f.hp)},
                                 {"team", std::to_string(f.team)}};
                           });
  return w;
}

int main() {
  printf("Running canonical snapshot tests...\n\n");
  World a = build({0, 1, 2}, 0);
  World b = build({2, 0, 1}, 100);
  CHECK(a.snap.encode() == b.snap.encode());
  CHECK(a.snap.hash() == b.snap.hash());
  CHECK(!snapshot::first_difference(a.snap, b.snap).has_value());
  CHECK(a.snap.entities[0].identity == "alpha");

  World changed = build({0, 1, 2}, 0, 21);
  CHECK(changed.snap.hash() != a.snap.hash());
  auto d = snapshot::first_difference(a.snap, changed.snap);
  CHECK(d.has_value());
  if (d) {
    CHECK(d->identity == "bravo");
    CHECK(d->field == "Fighter.hp");
    CHECK(d->left == "20" && d->right == "21");
    printf("  diff: %s\n", d->describe().c_str());
  }

  World missing = build({0, 2}, 0);
  auto d2 = snapshot::first_difference(a.snap, missing.snap);
  CHECK(d2.has_value() && d2->identity == "bravo" && d2->field == "<entity>");

  bool threw = false;
  try {
    std::vector<snapshot::EntityRecord> dup = {
        {.handle = EntityHandle{0, 1}, .entity_type = 0, .tags = {}, .cleanup = false},
        {.handle = EntityHandle{1, 1}, .entity_type = 0, .tags = {}, .cleanup = false}};
    (void)snapshot::canonicalize(dup, [](const EntityHandle &) { return std::string("same"); });
  } catch (const std::invalid_argument &) { threw = true; }
  CHECK(threw);

  printf("\n%d/%d tests passed.\n", passed, run);
  if (passed != run) { printf("SOME TESTS FAILED!\n"); return 1; }
  printf("ALL TESTS PASSED.\n");
  return 0;
}
