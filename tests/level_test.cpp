// level_test.cpp — level::Registry: register, load, mutate, write back.
#define FMT_HEADER_ONLY
#define AFTER_HOURS_USE_NONE
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/level.h>

#include <cstdio>
#include <string>

using namespace afterhours;

static int checks = 0, failures = 0;
static void check(bool c, const std::string &w) {
  ++checks;
  if (!c) { ++failures; std::printf("  FAIL: %s\n", w.c_str()); }
}

struct Pos : BaseComponent { float x = 0, y = 0; Pos() = default; Pos(float a, float b) : x(a), y(b) {} };
struct Label : BaseComponent { std::string text; int hp = 0; Label() = default; Label(std::string t, int h) : text(std::move(t)), hp(h) {} };

static level::Registry make_registry() {
  level::Registry reg;
  reg.register_type("crate",
      [](Entity &e, const level::Record &r) { e.addComponent<Pos>(r.get_float("x", 0), r.get_float("y", 0)); },
      [](const Entity &e, level::Record &r) { r.set("x", e.get<Pos>().x); r.set("y", e.get<Pos>().y); });
  reg.register_type("sign",
      [](Entity &e, const level::Record &r) {
        e.addComponent<Pos>(r.get_float("x", 0), r.get_float("y", 0));
        e.addComponent<Label>(r.get_string("text", ""), int(r.get_int("hp", 0)));
      },
      [](const Entity &e, level::Record &r) {
        r.set("x", e.get<Pos>().x); r.set("y", e.get<Pos>().y);
        r.set("text", e.get<Label>().text); r.set("hp", e.get<Label>().hp);
      });
  return reg;
}

int main() {
  level::Registry reg = make_registry();
  check(reg.size() == 2 && reg.has("crate") && !reg.has("ghost"), "registry holds two types");
  check(!reg.register_type("crate", [](Entity &, const level::Record &) {}, {}), "duplicate type rejected");
  check(!reg.register_type("", [](Entity &, const level::Record &) {}, {}), "empty type rejected");

  EntityCollection coll;
  level::Record crate; crate.type = "crate"; crate.set("x", 1.5); crate.set("y", 2.5);
  level::Record sign; sign.type = "sign"; sign.set("x", 3.0); sign.set("text", std::string("hi")); sign.set("hp", 7);
  level::Record ghost; ghost.type = "ghost"; ghost.set("x", 9.0);

  auto loaded = reg.load(coll, {crate, sign, ghost});
  check(!loaded.ok(), "unknown type makes load report errors");
  check(loaded.errors.size() == 1 && loaded.errors[0].kind == level::ErrorKind::UnknownType &&
        loaded.errors[0].type == "ghost" && loaded.errors[0].index == 2, "unknown type is a typed error, no crash");
  check(loaded.level.size() == 2, "known records still load");
  Entity *c = loaded.level.entries[0].entity;
  Entity *s = loaded.level.entries[1].entity;
  check(c->has<Pos>() && c->get<Pos>().x == 1.5f && c->get<Pos>().y == 2.5f, "crate components from record");
  check(s->has<Pos>() && s->has<Label>() && s->get<Label>().text == "hi" && s->get<Label>().hp == 7, "sign components from record");

  c->get<Pos>().x = 10.f;
  s->get<Label>().text = "bye";
  auto out = reg.write_back(loaded.level);
  check(out.ok() && out.records.size() == 2, "write-back emits both records");
  check(out.records[0].type == "crate" && out.records[0].get_float("x", 0) == 10.f, "mutated crate round-trips");
  check(out.records[1].type == "sign" && out.records[1].get_string("text", "") == "bye" &&
        out.records[1].get_int("hp", 0) == 7, "mutated sign round-trips");

  // Reloading the written records reproduces the mutated state.
  EntityCollection coll2;
  auto reloaded = reg.load(coll2, out.records);
  check(reloaded.ok() && reloaded.level.size() == 2 &&
        reloaded.level.entries[0].entity->get<Pos>().x == 10.f, "written records reload identically");

  level::Registry::unload(coll, loaded.level);
  check(loaded.level.empty(), "unload clears the loaded list");
  coll.cleanup();
  check(coll.get_entities().empty(), "unloaded entities are cleaned up");

  std::printf("level_test: %d/%d checks passed\n", checks - failures, checks);
  return failures > 0 ? 1 : 0;
}
