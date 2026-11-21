#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_SOKOL) && \
    !defined(AFTER_HOURS_USE_NONE)
#define AFTER_HOURS_USE_NONE 1
#endif
#include <afterhours/src/plugins/e2e_testing/runner.h>
#include <afterhours/src/plugins/e2e_testing/state_commands.h>

#include <cstdio>
#include <string>

using namespace afterhours;
using afterhours::testing::PendingE2ECommand;
namespace sc = afterhours::testing::state_commands;

static int checks_run = 0;
static int checks_passed = 0;
static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) checks_passed++;
  else fprintf(stderr, "  FAIL: %s\n", what.c_str());
}

static Entity dummy_entity;

static PendingE2ECommand run_expect(const std::string &name,
                                    const std::string &value) {
  PendingE2ECommand cmd;
  cmd.name = "expect_state";
  cmd.args = {name, value};
  sc::HandleExpectStateCommand{}.for_each_with(dummy_entity, cmd, 0.f);
  return cmd;
}

static PendingE2ECommand run_set(const std::string &name, const std::string &value) {
  PendingE2ECommand cmd;
  cmd.name = "set_state";
  cmd.args = {name, value};
  sc::HandleSetStateCommand{}.for_each_with(dummy_entity, cmd, 0.f);
  return cmd;
}

int main() {
  sc::clear_states();
  std::string screen = "menu";
  int level = 3;
  sc::register_state("screen", [&] { return screen; },
                     [&](const std::string &v) { screen = v; });
  sc::register_state_int("level", [&] { return level; },
                         [&](int v) { level = v; });
  sc::register_state("build", [] { return std::string("42"); });

  // A script driving changes then asserting them.
  check(run_set("screen", "gameplay").succeeded(), "set_state screen ok");
  check(screen == "gameplay", "set_state wrote screen");
  check(run_expect("screen", "gameplay").succeeded(), "expect_state screen passes");
  check(run_set("level", "7").succeeded(), "set_state level ok");
  check(level == 7, "set_state int wrote level");
  check(run_expect("level", "7").succeeded(), "expect_state int passes as decimal");

  // Mismatch retries (wait_until / timeout machinery owns the verdict).
  {
    auto c = run_expect("screen", "menu");
    check(c.is_retry() && c.last_mismatch.find("gameplay") != std::string::npos,
          "expect_state mismatch retries naming actual value");
  }

  // Unknown state fails by name and lists what is registered.
  {
    auto c = run_expect("bogus_state", "x");
    check(c.is_consumed() && !c.succeeded(), "expect_state unknown fails");
    check(c.error_message.find("unknown state 'bogus_state'") != std::string::npos,
          "expect_state unknown names the state");
    check(c.error_message.find("screen") != std::string::npos,
          "expect_state unknown lists registered states");
    auto s = run_set("bogus_state", "x");
    check(!s.succeeded() && s.error_message.find("unknown state 'bogus_state'") != std::string::npos,
          "set_state unknown fails by name");
  }

  // Read-only state: expect works, set fails by name.
  {
    check(run_expect("build", "42").succeeded(), "expect_state read-only passes");
    auto s = run_set("build", "9");
    check(!s.succeeded() && s.error_message.find("read-only") != std::string::npos,
          "set_state read-only fails by name");
  }

  // Malformed commands fail immediately with usage.
  {
    PendingE2ECommand c;
    c.name = "expect_state";
    c.args = {"screen"};
    sc::HandleExpectStateCommand{}.for_each_with(dummy_entity, c, 0.f);
    check(!c.succeeded() && c.error_message.find("requires") != std::string::npos,
          "expect_state without value fails with usage");
  }

  // Script parsing: two single-token args, no runner changes needed.
  {
    const char *path = "/tmp/afterhours_state_commands_test.e2e";
    FILE *f = fopen(path, "w");
    fputs("set_state screen gameplay\nexpect_state screen gameplay\n", f);
    fclose(f);
    auto cmds = testing::parse_script(path);
    check(cmds.size() == 2, "state script parses two commands");
    if (cmds.size() == 2) {
      check(cmds[0].args.size() == 2 && cmds[0].args[0] == "screen" &&
                cmds[0].args[1] == "gameplay",
            "set_state parses name+value");
      check(cmds[1].args.size() == 2, "expect_state parses name+value");
    }
    remove(path);
  }

  sc::clear_states();
  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) return 1;
  printf("All checks passed!\n");
  return 0;
}
