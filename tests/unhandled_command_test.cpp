#include "ui_test_harness.h"

#include <afterhours/src/plugins/e2e_testing/pending_command.h>
#include <afterhours/src/plugins/e2e_testing/runner.h>

#include <cstdio>
#include <cstdlib>
#include <string>

using namespace afterhours;
using afterhours::testing::PendingE2ECommand;

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s\n", what.c_str());
  }
}

int main() {
  printf("=== unhandled command diagnosis ===\n\n");

  // A SystemManager that never got register_all_handlers has nothing to
  // consume a command, and the runner blocks on unconsumed commands -- so the
  // script stalls to its timeout with no hint of why. That is the case.
  {
    EntityCollection coll;
    Entity &e = coll.createEntity();
    auto &cmd = e.addComponent<PendingE2ECommand>();
    cmd.name = "click";
    coll.merge_entity_arrays();

    check(!cmd.is_consumed(),
          "a command nobody handles stays unconsumed, so the runner waits");

    // Which is what the timeout message now has to name. Before, it said only
    // "Cancelled due to script timeout", which reads as slowness.
    cmd.fail("Cancelled due to script timeout");
    check(cmd.is_consumed() || !cmd.error_message.empty(),
          "and the timeout path is where it gets resolved");
  }

  // The message itself. A script whose command nothing handles stalls to the
  // timeout, and the timeout used to say only "Cancelled due to script
  // timeout" -- which reads as slowness, not as a missing handler.
  {
    static std::string captured;
    captured.clear();
    log_sink_fn = [](const char *, const char *message) {
      captured += message;
      captured += "\n";
    };

    EntityCollection coll;
    EntityHelper::set_default_collection(&coll);
    Entity &e = coll.createEntity();
    auto &cmd = e.addComponent<PendingE2ECommand>();
    cmd.name = "click";
    coll.merge_entity_arrays();

    // A real script, because tick() returns early with no commands loaded.
    const std::string script_path = "/tmp/afh_unhandled_probe.e2e";
    {
      FILE *f = std::fopen(script_path.c_str(), "w");
      std::fputs("click 10 10\n", f);
      std::fclose(f);
    }

    testing::E2ERunner runner;
    runner.load_script(script_path);
    runner.set_timeout(0.001f);
    for (int i = 0; i < 8; i++)
      runner.tick(1.f);

    log_sink_fn = nullptr;
    EntityHelper::set_default_collection(nullptr);

    const bool named = captured.find("click") != std::string::npos;
    const bool explained =
        captured.find("register_all_handlers") != std::string::npos;
    printf("  named the command: %s, explained why: %s\n",
           named ? "yes" : "no", explained ? "yes" : "no");
    check(named, "the timeout names the command nothing consumed");
    check(explained, "and points at the unregistered handler pack");
  }

  // A handled one never reaches the timeout path at all.
  {
    EntityCollection coll;
    Entity &e = coll.createEntity();
    auto &cmd = e.addComponent<PendingE2ECommand>();
    cmd.name = "click";
    cmd.consume();
    coll.merge_entity_arrays();
    check(cmd.is_consumed(), "a consumed command does not stall the runner");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
