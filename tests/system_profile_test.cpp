// system_profile_test.cpp
// These assert coverage, not speed: a system the test did not write shows up,
// named, in the right phase.
//
// Before the hook a consumer could only time its own systems, so a per-draw
// regression in the library had to be found by bisect.

#include "ui_test_harness.h"

#include <afterhours/src/plugins/e2e_testing/perf_commands.h>

#include <string>
#include <vector>

using namespace afterhours;

namespace {

struct Marker : BaseComponent {};

// Stands in for a library-provided system: the test never names it, so the
// name has to come from the default.
struct UnnamedUpdateSystem : System<Marker> {
  int seen = 0;
  void for_each_with(Entity &, Marker &, float) override { seen++; }
};

struct UnnamedRenderSystem : System<Marker> {
  void for_each_with(Entity &, Marker &, float) override {}
};

// A consumer that wants something friendlier than the type name.
struct RenamedSystem : System<Marker> {
  std::string_view name() const override { return "friendly-name"; }
  void for_each_with(Entity &, Marker &, float) override {}
};

struct Record {
  std::string name;
  SystemPhase phase;
  bool is_begin;
};

std::vector<Record> &log() {
  static std::vector<Record> v;
  return v;
}

void install_logging_hook() {
  log().clear();
  SystemManager::set_profile_hook(SystemProfileHook{
      [](std::string_view n, SystemPhase p) {
        log().push_back({std::string(n), p, true});
      },
      [](std::string_view n, SystemPhase p) {
        log().push_back({std::string(n), p, false});
      }});
}

void clear_hook() { SystemManager::set_profile_hook(SystemProfileHook{}); }

bool saw(const std::string &needle, SystemPhase phase) {
  for (const auto &r : log())
    if (r.name.find(needle) != std::string::npos && r.phase == phase &&
        r.is_begin)
      return true;
  return false;
}

} // namespace

TEST(a_system_reports_its_own_type_name) {
  UnnamedUpdateSystem sys;
  const std::string n(sys.name());
  printf("  default name: %s\n", n.c_str());
  // Demangled, not the mangled spelling typeid hands back.
  CHECK(n.find("UnnamedUpdateSystem") != std::string::npos);
}

TEST(a_system_can_override_its_name) {
  RenamedSystem sys;
  CHECK(std::string(sys.name()) == "friendly-name");
}

TEST(the_hook_brackets_systems_in_every_phase) {
  install_logging_hook();

  SystemManager sm;
  sm.register_update_system(std::make_unique<UnnamedUpdateSystem>());
  sm.register_render_system(std::make_unique<UnnamedRenderSystem>());
  sm.register_fixed_update_system(std::make_unique<RenamedSystem>());

  Entities entities;
  sm.tick(entities, 0.016f);
  sm.fixed_tick(entities, 0.016f);
  sm.render(entities, 0.016f);

  printf("  %zu bracket events\n", log().size());
  CHECK(saw("UnnamedUpdateSystem", SystemPhase::Update));
  CHECK(saw("UnnamedRenderSystem", SystemPhase::Render));
  CHECK(saw("friendly-name", SystemPhase::FixedUpdate));

  // Every begin is matched, or a profiler built on this leaks open frames.
  int depth = 0, max_depth = 0;
  for (const auto &r : log()) {
    depth += r.is_begin ? 1 : -1;
    max_depth = std::max(max_depth, depth);
    CHECK(depth >= 0);
  }
  CHECK(depth == 0);
  CHECK(max_depth == 1); // one system at a time, never nested
  clear_hook();
}

TEST(no_hook_means_no_events) {
  clear_hook();
  log().clear();

  SystemManager sm;
  sm.register_update_system(std::make_unique<UnnamedUpdateSystem>());
  Entities entities;
  sm.tick(entities, 0.016f);

  CHECK(log().empty());
}


// The point of the hook is that dump_profile works without the consumer
// writing a profiler. puzzle wrote 518 lines of one and still saw no library
// systems; this asserts the library can now answer for itself.
TEST(the_builtin_profile_names_systems_the_consumer_never_wrote) {
  afterhours::testing::perf_commands::builtin_profile::enable();

  SystemManager sm;
  sm.register_update_system(std::make_unique<UnnamedUpdateSystem>());
  sm.register_render_system(std::make_unique<UnnamedRenderSystem>());

  Entities entities;
  for (int frame = 0; frame < 3; frame++) {
    sm.tick(entities, 0.016f);
    sm.render(entities, 0.016f);
  }

  auto entries = afterhours::testing::perf_commands::provider().top_entries(10);
  printf("  %zu systems timed\n", entries.size());
  CHECK(entries.size() >= 2);

  bool found_update = false, found_render = false;
  for (const auto &e : entries) {
    if (e.name.find("UnnamedUpdateSystem") != std::string::npos) {
      found_update = true;
      // Three frames, so three calls -- a profile that loses calls is worse
      // than none.
      CHECK(e.entity_count.value_or(0) == 3);
    }
    if (e.name.find("UnnamedRenderSystem") != std::string::npos)
      found_render = true;
  }
  CHECK(found_update);
  CHECK(found_render);

  afterhours::testing::perf_commands::builtin_profile::disable();
}


// A system is allowed to clear the hook while it is running: a UI with a
// "stop profiling" button does exactly that. The scope bracketing it must not
// then call a cleared std::function, which throws out of a destructor and
// terminates rather than failing. This crashed the profile lab on the first
// click of stop.
TEST(a_system_may_clear_the_hook_while_it_runs) {
  struct SelfDisablingSystem : System<Marker> {
    void for_each_with(Entity &, Marker &, float) override {
      SystemManager::set_profile_hook(SystemProfileHook{});
    }
  };

  install_logging_hook();
  SystemManager sm;
  sm.register_update_system(std::make_unique<SelfDisablingSystem>());

  Entities entities;
  entities.push_back(std::make_shared<Entity>());
  entities.back()->addComponent<Marker>();

  sm.tick(entities, 0.016f); // used to terminate here
  CHECK(true);
  clear_hook();
}

int main() { return ui_test::run_registered_tests("system profile"); }
