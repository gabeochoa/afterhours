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
      [](Entity &e, const level::Json &r) { e.addComponent<Pos>(r.value<float>("x", 0.f), r.value<float>("y", 0.f)); },
      [](const Entity &e, level::Json &out) { out["x"] = e.get<Pos>().x; out["y"] = e.get<Pos>().y; });
  reg.register_type("sign",
      [](Entity &e, const level::Json &r) {
        e.addComponent<Pos>(r.value<float>("x", 0.f), r.value<float>("y", 0.f));
        e.addComponent<Label>(r.value<std::string>("text", ""), r.value<int>("hp", 0));
      },
      [](const Entity &e, level::Json &out) {
        out["x"] = e.get<Pos>().x; out["y"] = e.get<Pos>().y;
        out["text"] = e.get<Label>().text; out["hp"] = e.get<Label>().hp;
      });
  return reg;
}

int main() {
  level::Registry reg = make_registry();
  check(reg.size() == 2 && reg.has("crate") && !reg.has("ghost"), "registry holds two types");
  check(!reg.register_type("crate", [](Entity &, const level::Json &) {}, {}), "duplicate type rejected");
  check(!reg.register_type("", [](Entity &, const level::Json &) {}, {}), "empty type rejected");

  EntityCollection coll;
  auto records = level::Json::parse(R"([
    {"type": "crate", "x": 1.5, "y": 2.5},
    {"type": "sign", "x": 3.0, "text": "hi", "hp": 7},
    {"type": "ghost", "x": 9.0},
    {"x": 1.0},
    {"type": 42},
    7
  ])");
  auto loaded = reg.load(coll, records);
  check(!loaded.ok(), "bad records make load report errors");
  check(loaded.errors.size() == 4, "unknown, missing, non-string and non-object records all error");
  check(loaded.errors[0].kind == level::ErrorKind::UnknownType &&
        loaded.errors[0].type == "ghost" && loaded.errors[0].index == 2, "unknown type is a typed error, no crash");
  check(loaded.errors[1].kind == level::ErrorKind::MalformedRecord && loaded.errors[1].index == 3, "missing type is malformed, skipped");
  check(loaded.errors[2].kind == level::ErrorKind::MalformedRecord && loaded.errors[2].index == 4, "non-string type is malformed, skipped");
  check(loaded.errors[3].kind == level::ErrorKind::MalformedRecord && loaded.errors[3].index == 5, "non-object record is malformed, skipped");
  check(loaded.level.size() == 2, "known records still load");
  Entity *c = loaded.level.entries[0].entity;
  Entity *s = loaded.level.entries[1].entity;
  check(c->has<Pos>() && c->get<Pos>().x == 1.5f && c->get<Pos>().y == 2.5f, "crate components from JSON");
  check(s->has<Pos>() && s->has<Label>() && s->get<Label>().text == "hi" && s->get<Label>().hp == 7, "sign components from JSON");

  EntityCollection coll_bad;
  auto not_array = reg.load(coll_bad, level::Json::parse(R"({"type": "crate", "x": 1.0})"));
  check(not_array.level.empty() && not_array.errors.size() == 1 &&
        not_array.errors[0].kind == level::ErrorKind::MalformedRecord, "non-array records rejected as one error");

  level::Registry fragile_reg;
  fragile_reg.register_type("fragile",
      [](Entity &e, const level::Json &r) { e.addComponent<Pos>(r.value<float>("x", 0.f), 0.f); }, {});
  EntityCollection coll_fragile;
  auto bad_field = fragile_reg.load(coll_fragile, level::Json::parse(R"([{"type": "fragile", "x": "wide"}])"));
  check(bad_field.level.empty() && bad_field.errors.size() == 1 &&
        bad_field.errors[0].kind == level::ErrorKind::InvalidField, "wrong-typed field surfaces as typed error, no throw");
  coll_fragile.cleanup();
  check(coll_fragile.get_entities().empty(), "failed construct leaves no live entity");

  c->get<Pos>().x = 10.f;
  s->get<Label>().text = "bye";
  auto out = reg.write_back(loaded.level);
  check(out.ok() && out.records.is_array() && out.records.size() == 2, "write-back emits a JSON array");
  check(out.records[0].value<std::string>("type", "") == "crate" && out.records[0].value<float>("x", 0.f) == 10.f, "mutated crate round-trips");
  check(out.records[1].value<std::string>("type", "") == "sign" && out.records[1].value<std::string>("text", "") == "bye" &&
        out.records[1].value<int>("hp", 0) == 7, "mutated sign round-trips");

  EntityCollection coll2;
  auto reparsed = level::Json::parse(out.records.dump());
  auto reloaded = reg.load(coll2, reparsed);
  check(reloaded.ok() && reloaded.level.size() == 2 &&
        reloaded.level.entries[0].entity->get<Pos>().x == 10.f, "dumped records parse and reload identically");

  level::Registry::unload(coll, loaded.level);
  check(loaded.level.empty(), "unload clears the loaded list");
  coll.cleanup();
  check(coll.get_entities().empty(), "unloaded entities are cleaned up");

  std::printf("level_test: %d/%d checks passed\n", checks - failures, checks);
  return failures > 0 ? 1 : 0;
}
