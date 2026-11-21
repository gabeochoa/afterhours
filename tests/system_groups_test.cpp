
#include "ui_test_harness.h"

#include <string>
#include <vector>

using namespace afterhours;

namespace {

struct Marker : BaseComponent {};

std::vector<int> &runs() {
    static std::vector<int> v;
    return v;
}

struct Recorder : System<Marker> {
    int id = 0;
    bool gate = true;
    std::string label;
    std::string_view name() const override { return label; }
    bool should_run(float) const override { return gate; }
    void once(float) override { runs().push_back(id); }
    void for_each_with(Entity &, Marker &, float) override {}
};

struct MutableGate : System<Marker> {
    int *runs;
    explicit MutableGate(int &r) : runs(&r) {}
    bool should_run(float) override { return false; }
    void once(float) override { ++*runs; }
    void for_each_with(Entity &, Marker &, float) override {}
};

std::unique_ptr<Recorder> rec(int id, bool gate = true,
                              const std::string &label = "rec") {
    auto r = std::make_unique<Recorder>();
    r->id = id;
    r->gate = gate;
    r->label = label;
    return r;
}

bool ran(int id) {
    for (int r : runs())
        if (r == id) return true;
    return false;
}

}

TEST(a_disabled_group_gates_exactly_its_systems) {
    runs().clear();
    SystemManager sm;
    sm.register_update_system(rec(1), "world");
    sm.register_update_system(rec(2), "world");
    sm.register_update_system(rec(3), "audio");
    sm.register_update_system(rec(4));

    sm.set_group_enabled("world", false);
    Entities entities;
    sm.tick(entities, 0.016f);

    CHECK(!ran(1));
    CHECK(!ran(2));
    CHECK(ran(3));
    CHECK(ran(4));
}

TEST(group_gate_composes_with_should_run) {
    runs().clear();
    SystemManager sm;
    sm.register_update_system(rec(1, false), "world");
    sm.register_update_system(rec(2, true), "audio");
    sm.set_group_enabled("audio", false);

    Entities entities;
    sm.tick(entities, 0.016f);

    CHECK(!ran(1));
    CHECK(!ran(2));
}

TEST(disabling_a_group_preserves_run_order) {
    runs().clear();
    SystemManager sm;
    sm.register_update_system(rec(1), "a");
    sm.register_update_system(rec(2), "b");
    sm.register_update_system(rec(3));
    sm.register_update_system(rec(4), "b");
    sm.register_update_system(rec(5), "a");

    sm.set_group_enabled("b", false);
    Entities entities;
    sm.tick(entities, 0.016f);

    CHECK((runs() == std::vector<int>{1, 3, 5}));

    runs().clear();
    sm.set_group_enabled("b", true);
    sm.tick(entities, 0.016f);
    CHECK((runs() == std::vector<int>{1, 2, 3, 4, 5}));
}

TEST(groups_are_enabled_by_default_and_queryable) {
    runs().clear();
    SystemManager sm;
    sm.register_update_system(rec(1, true, "alpha"), "world");
    sm.register_update_system(rec(2, true, "beta"), "world");
    sm.register_update_system(rec(3, true, "gamma"), "audio");

    CHECK(sm.group_enabled("world"));
    CHECK(sm.group_enabled("never-registered"));

    const auto world = sm.systems_in_group("world");
    CHECK(world.size() == 2);
    CHECK(world[0] == "alpha");
    CHECK(world[1] == "beta");
    CHECK(sm.systems_in_group("none").empty());

    sm.set_group_enabled("world", false);
    CHECK(!sm.group_enabled("world"));
    CHECK(sm.group_enabled("audio"));
}

TEST(disabling_a_group_before_registration_still_gates) {
    runs().clear();
    SystemManager sm;
    sm.set_group_enabled("world", false);
    sm.register_update_system(rec(1), "world");
    sm.register_update_system(rec(2));

    Entities entities;
    sm.tick(entities, 0.016f);

    CHECK(!ran(1));
    CHECK(ran(2));
}

TEST(groups_gate_every_phase) {
    runs().clear();
    SystemManager sm;
    sm.register_update_system(rec(1), "off");
    sm.register_fixed_update_system(rec(2), "off");
    sm.register_render_system(rec(3), "off");
    sm.set_group_enabled("off", false);

    Entities entities;
    sm.tick(entities, 0.016f);
    sm.fixed_tick(entities, 0.016f);
    sm.render(entities, 0.016f);

    CHECK(runs().empty());
}

TEST(group_gate_honors_a_non_const_should_run) {
    runs().clear();
    int mutable_runs = 0;
    SystemManager sm;
    sm.register_update_system(std::make_unique<MutableGate>(mutable_runs),
                              "world");
    sm.register_update_system(rec(1), "world");

    Entities entities;
    sm.tick(entities, 0.016f);

    CHECK(mutable_runs == 0);
    CHECK(ran(1));
}

int main() { return ui_test::run_registered_tests("system groups"); }
