#define AFTER_HOURS_ENABLE_E2E_TESTING
#include "ui_test_harness.h"
#include <afterhours/src/plugins/e2e_testing/e2e_testing.h>

using namespace afterhours;
using namespace afterhours::testing;

struct SpawnEnemy : System<PendingE2ECommand> {
  void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (!cmd.is_consumed() && cmd.is("spawn_enemy")) cmd.consume();
  }
};

struct States {
  EntityCollection collection;
  SystemManager pack, menu, gameplay;

  States() {
    EntityHelper::set_default_collection(&collection);
    reset_command_error_count();
    register_unknown_handler(pack, {"spawn_enemy"});
    register_cleanup(pack);
    register_shared_handlers(menu, pack);
    gameplay.register_update_system(std::make_unique<SpawnEnemy>());
    register_shared_handlers(gameplay, pack);
  }
  ~States() { EntityHelper::set_default_collection(nullptr); }

  PendingE2ECommand &send(const std::string &name) {
    auto &cmd = collection.createEntity().addComponent<PendingE2ECommand>();
    cmd.name = name;
    collection.merge_entity_arrays();
    return cmd;
  }
};

TEST(a_deferred_command_waits_for_the_state_that_handles_it) {
  States s;
  s.send("spawn_enemy");
  for (int i = 0; i < 5; ++i) s.menu.run(1.f / 60.f);
  CHECK(get_command_error_count() == 0);
  s.gameplay.run(1.f / 60.f);
  CHECK(get_command_error_count() == 0);
  CHECK(s.collection.get_entities().empty());
}

TEST(other_unknown_commands_still_fail_at_once) {
  States s;
  auto &cmd = s.send("spwan_enemy");
  s.menu.run(1.f / 60.f);
  CHECK(get_command_error_count() == 1);
  (void)cmd;
}

TEST(a_deferred_command_that_never_lands_times_out_with_a_reason) {
  static std::string captured;
  captured.clear();
  const auto previous = log_sink_fn;
  log_sink_fn = [](const char *, const char *message) { captured += message; };
  States s;
  s.send("spawn_enemy");
  for (int i = 0; i <= PendingE2ECommand::MAX_FRAMES + 1; ++i)
    s.menu.run(1.f / 60.f);
  log_sink_fn = previous;
  CHECK(get_command_error_count() == 1);
  CHECK(captured.find("'spawn_enemy' waited for a state that handles it") !=
        std::string::npos);
}

int main() { return ui_test::run_registered_tests("E2E deferred unknown"); }
