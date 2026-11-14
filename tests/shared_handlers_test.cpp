// shared_handlers_test.cpp
// register_shared_handlers: one pack of e2e handlers, run by whichever
// SystemManager ticks, so a command is handled in every state without
// registering its handler per manager.

#define FMT_HEADER_ONLY
#define AFTER_HOURS_USE_NONE
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/e2e_testing/e2e_testing.h>

#include <cstdio>
#include <memory>
#include <string>

using namespace afterhours;
using afterhours::testing::PendingE2ECommand;

static int checks = 0;
static int failures = 0;

static void check(bool cond, const std::string &what) {
  ++checks;
  if (!cond) {
    ++failures;
    std::printf("  FAIL: %s\n", what.c_str());
  }
}

static int handled = 0;

struct HandleSaveCheck : System<PendingE2ECommand> {
  void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_save")) return;
    ++handled;
    cmd.consume();
  }
};

static PendingE2ECommand &send(const std::string &name) {
  Entity &e = EntityHelper::createEntity();
  auto &cmd = e.addComponent<PendingE2ECommand>();
  cmd.name = name;
  EntityHelper::merge_entity_arrays();
  return cmd;
}

static int pending_count() {
  int n = 0;
  for (Entity &e : EntityQuery().whereHasComponent<PendingE2ECommand>().gen())
    if (!e.get<PendingE2ECommand>().is_consumed()) n++;
  return n;
}

int main() {
  SystemManager pack;
  pack.register_update_system(std::make_unique<HandleSaveCheck>());
  testing::register_unknown_handler(pack);
  testing::register_cleanup(pack);

  SystemManager menu, game;
  testing::register_shared_handlers(menu, pack);
  testing::register_shared_handlers(game, pack);

  testing::reset_command_error_count();
  send("expect_save");
  menu.run(1.f / 60.f);
  check(handled == 1, "the menu manager runs the shared handler");
  check(pending_count() == 0, "the shared cleanup removes the command");

  send("expect_save");
  game.run(1.f / 60.f);
  check(handled == 2, "the game manager runs the same handler");

  send("no_such_command");
  game.run(1.f / 60.f);
  check(testing::get_command_error_count() == 1,
        "the pack's unknown handler rejects a command nothing handles");
  check(handled == 2, "an unrelated command does not reach the handler");

  std::printf("shared_handlers_test: %d/%d checks passed\n", checks - failures,
              checks);
  return failures > 0 ? 1 : 0;
}
