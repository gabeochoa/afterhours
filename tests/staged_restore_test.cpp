
#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>

#include <cstdio>
#include <stdexcept>
#include <vector>

using namespace afterhours;

static int checks = 0, failures = 0;
static void check(bool c, const char *e, int l) {
  checks++;
  if (!c) { failures++; std::fprintf(stderr, "  FAIL: %s (line %d)\n", e, l); }
}
#define CHECK(...) check((__VA_ARGS__), #__VA_ARGS__, __LINE__)

struct Wallet : BaseComponent { int coins = 0; };
struct Health : BaseComponent { int hp = 0; };

struct Snapshot { int wallet; std::vector<int> hps; };

static void build(EntityCollection &c, const Snapshot &s) {
  Entity &w = c.createEntity();
  w.addComponent<Wallet>().coins = s.wallet;
  c.registerSingleton<Wallet>(w);
  for (int hp : s.hps) c.createEntity().addComponent<Health>().hp = hp;
  c.merge_entity_arrays();
}

static bool valid(EntityCollection &c) {
  if (!c.has_singleton<Wallet>() || c.get_singleton_cmp<Wallet>()->coins < 0)
    return false;
  for (auto &sp : c.get_entities())
    if (sp && sp->has<Health>() && sp->get<Health>().hp < 0) return false;
  return true;
}

static std::vector<int> hps(EntityCollection &c) {
  std::vector<int> out;
  for (auto &sp : c.get_entities())
    if (sp && sp->has<Health>()) out.push_back(sp->get<Health>().hp);
  return out;
}

int main() {
  EntityCollection live;
  build(live, {.wallet = 10, .hps = {5, 7}});
  EntityHandle old_handle = live.handle_for(*live.get_entities()[0]);
  CHECK(old_handle.valid());

  StagedRestore restore;
  build(restore.staging(), {.wallet = 99, .hps = {1, 2, 3}});
  CHECK(live.get_singleton_cmp<Wallet>()->coins == 10);
  CHECK(hps(live) == std::vector<int>({5, 7}));
  CHECK(restore.commit_if(live, valid));
  CHECK(live.get_singleton_cmp<Wallet>()->coins == 99);
  CHECK(hps(live) == std::vector<int>({1, 2, 3}));
  CHECK(!restore.commit(live));

  bool ok = staged_restore(live, [](EntityCollection &c) {
    build(c, {.wallet = 50, .hps = {3, -1}});
  }, valid);
  CHECK(!ok);
  CHECK(live.get_singleton_cmp<Wallet>()->coins == 99);
  CHECK(hps(live) == std::vector<int>({1, 2, 3}));

  ok = staged_restore(live, [](EntityCollection &c) {
    build(c, {.wallet = 1, .hps = {9}});
    throw std::runtime_error("decode failed");
  }, valid);
  CHECK(!ok);
  CHECK(live.get_singleton_cmp<Wallet>()->coins == 99);
  CHECK(hps(live) == std::vector<int>({1, 2, 3}));

  {
    StagedRestore r;
    build(r.staging(), {.wallet = 123, .hps = {42}});
    r.discard();
  }
  CHECK(live.get_singleton_cmp<Wallet>()->coins == 99);

  std::printf("%d/%d checks passed\n", checks - failures, checks);
  return failures ? 1 : 0;
}
