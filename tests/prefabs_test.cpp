#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/src/plugins/prefabs.h>

#include <cstdio>
#include <string>

using namespace afterhours;

static int run = 0, passed = 0;
static void check(bool c, const char *e, int l) {
  run++;
  if (c) passed++; else fprintf(stderr, "  FAIL: %s (line %d)\n", e, l);
}
#define CHECK(e) check((e), #e, __LINE__)

struct Transform : BaseComponent { float x = 0, y = 0; };
struct Health : BaseComponent { int hp = 10; };
struct Label : BaseComponent { std::string text; };

int main() {
  prefabs::clear();
  prefabs::define("ice_machine", [](Entity &e) {
    e.addComponent<Transform>().x = 1;
    e.addComponent<Health>(); // hp 10
  });
  CHECK(prefabs::has("ice_machine"));
  CHECK(prefabs::names().size() == 1);

  EntityCollection ec;
  { // bundle components present
    auto e = prefabs::spawn(ec, "ice_machine");
    CHECK(e.has_value());
    CHECK(e.asE().has<Transform>() && e.asE().has<Health>());
    CHECK(e.asE().get<Health>().hp == 10);
  }
  { // override wins over bundle default
    auto e = prefabs::spawn(ec, "ice_machine", [](Entity &ent) {
      ent.get<Health>().hp = 99;
      ent.addComponent<Label>().text = "custom";
    });
    CHECK(e.asE().get<Health>().hp == 99);
    CHECK(e.asE().has<Label>());
  }
  { // unknown name fails clean, creates nothing
    size_t before = ec.get_entities().size() + ec.get_temp().size();
    auto e = prefabs::spawn(ec, "nope");
    CHECK(!e.has_value());
    CHECK(ec.get_entities().size() + ec.get_temp().size() == before);
  }
  { // two spawns independent
    auto a = prefabs::spawn(ec, "ice_machine");
    auto b = prefabs::spawn(ec, "ice_machine");
    a.asE().get<Health>().hp = 1;
    CHECK(b.asE().get<Health>().hp == 10);
    CHECK(a.asE().id != b.asE().id);
  }
  { // isolated Registry + registration order
    prefabs::Registry local;
    local.define("b", [](Entity &e) { e.addComponent<Label>(); });
    local.define("a", [](Entity &e) { e.addComponent<Health>(); });
    CHECK(local.names() == std::vector<std::string>({"b", "a"}));
    CHECK(!local.spawn(ec, "ice_machine").has_value());
  }
  { // re-define replaces, keeps position
    prefabs::Registry local;
    local.define("b", [](Entity &e) { e.addComponent<Transform>().x = 1; });
    local.define("a", [](Entity &e) { e.addComponent<Health>(); });
    local.define("b", [](Entity &e) { e.addComponent<Transform>().x = 2; });
    CHECK(local.names() == std::vector<std::string>({"b", "a"}));
    auto e = local.spawn(ec, "b");
    CHECK(e.has_value());
    CHECK(e.asE().get<Transform>().x == 2);
  }

  prefabs::clear();
  printf("%d/%d checks passed\n", passed, run);
  return passed == run ? 0 : 1;
}
