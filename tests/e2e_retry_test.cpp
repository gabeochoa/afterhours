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
  printf("=== retrying-assert helper ===\n\n");

  // retry_with() marks Retry and stashes the mismatch; reset_retry() returns
  // to Ready and keeps the latest mismatch; plain retry() stashes nothing.
  {
    EntityCollection coll;
    Entity &e = coll.createEntity();
    auto &cmd = e.addComponent<PendingE2ECommand>();
    cmd.name = "assert_ui";
    check(cmd.last_mismatch.empty(), "fresh command has no mismatch");
    cmd.retry();
    check(cmd.is_retry(), "plain retry() marks Retry");
    check(cmd.last_mismatch.empty(),
          "plain retry() leaves no mismatch to report");
    cmd.reset_retry();
    cmd.retry_with("x=1 but got 2");
    check(cmd.is_retry(), "retry_with() marks Retry");
    check(cmd.last_mismatch == "x=1 but got 2",
          "retry_with() stashes the mismatch");
    cmd.reset_retry();
    check(!cmd.is_retry() && !cmd.is_consumed(),
          "reset_retry() returns to Ready without consuming");
    check(cmd.last_mismatch == "x=1 but got 2",
          "reset_retry() keeps the latest mismatch");
    cmd.retry_with("x=1 but got 3");
    check(cmd.last_mismatch == "x=1 but got 3",
          "a later retry_with() overwrites with the latest mismatch");
  }

  // Mismatch-then-match: the handler retries while wrong and consumes once
  // right, instead of failing on first evaluation.
  {
    EntityCollection coll;
    Entity &e = coll.createEntity();
    auto &cmd = e.addComponent<PendingE2ECommand>();
    cmd.name = "assert_ui";
    cmd.retry_with("x=1 but got 2");
    cmd.reset_retry();
    cmd.retry_with("x=1 but got 2");
    cmd.reset_retry();
    cmd.consume();
    check(cmd.is_consumed() && cmd.succeeded(),
          "a late-matching assertion consumes instead of failing");
  }

  // Timeout reports the last mismatch, not the generic timeout line.
  {
    static std::string captured;
    captured.clear();
    log_sink_fn = [](const char *, const char *message) {
      captured += message;
      captured += "\n";
    };

    EntityCollection coll;
    Entity &e = coll.createEntity();
    auto &cmd = e.addComponent<PendingE2ECommand>();
    cmd.name = "assert_ui";
    cmd.args = {"some_widget"};
    testing::reset_command_error_count();
    testing::E2ECommandCleanupSystem cleanup;
    for (int i = 0; i <= PendingE2ECommand::MAX_FRAMES; i++) {
      cmd.retry_with("assert_ui 'some_widget': x=1 but got 2");
      cleanup.for_each_with(e, cmd, 0.f);
    }

    log_sink_fn = nullptr;
    check(cmd.is_consumed() && !cmd.succeeded(),
          "a never-matching assertion fails at the timeout");
    check(cmd.error_message.find("x=1 but got 2") != std::string::npos,
          "the timeout error carries the last mismatch");
    check(cmd.error_message.find("timed out after 30 frames") !=
              std::string::npos,
          "the timeout error still names the timeout");
    check(testing::get_command_error_count() == 1,
          "the timeout counts as one error");
    check(captured.find("x=1 but got 2") != std::string::npos,
          "the timeout log carries the last mismatch");
  }

  // Plain retry() timeouts keep the generic message: no behaviour change
  // for handlers that do not use the helper.
  {
    EntityCollection coll;
    Entity &e = coll.createEntity();
    auto &cmd = e.addComponent<PendingE2ECommand>();
    cmd.name = "expect_slider";
    testing::reset_command_error_count();
    testing::E2ECommandCleanupSystem cleanup;
    for (int i = 0; i <= PendingE2ECommand::MAX_FRAMES; i++) {
      cmd.retry();
      cleanup.for_each_with(e, cmd, 0.f);
    }
    check(cmd.is_consumed() && !cmd.succeeded(),
          "a plain-retry command still fails at the timeout");
    check(cmd.error_message.find("Command 'expect_slider'") !=
              std::string::npos,
          "a plain-retry timeout keeps the generic message");
  }

  // A timeout nothing retried says the handler is probably not registered.
  {
    EntityCollection coll;
    Entity &e = coll.createEntity();
    auto &cmd = e.addComponent<PendingE2ECommand>();
    cmd.name = "expect_autosave";
    testing::E2ECommandCleanupSystem cleanup;
    for (int i = 0; i <= PendingE2ECommand::MAX_FRAMES; i++)
      cleanup.for_each_with(e, cmd, 0.f);
    check(cmd.error_message.find("is its handler registered") !=
              std::string::npos,
          "an untouched timeout asks whether the handler is registered");
  }

  // wait_until: a failing check is rearmed until it passes.
  {
    EntityCollection coll;
    Entity &e = coll.createEntity();
    auto &cmd = e.addComponent<PendingE2ECommand>();
    cmd.name = "expect_rope_state";
    cmd.wait_until_seconds = cmd.retry_budget = 1.f;
    testing::reset_command_error_count();
    testing::E2ECommandCleanupSystem cleanup;
    for (int i = 0; i < 40; i++) {
      cmd.fail("rope is Holding, expected Measuring");
      cleanup.for_each_with(e, cmd, 1.f / 60.f);
    }
    check(!cmd.is_consumed() && !e.cleanup,
          "a failing wait_until check is rearmed, not cleaned up");
    check(cmd.last_mismatch == "rope is Holding, expected Measuring",
          "the rearmed failure is kept as the last mismatch");
    check(testing::get_command_error_count() == 0,
          "rearmed failures are not counted");
    cmd.consume();
    cleanup.for_each_with(e, cmd, 1.f / 60.f);
    check(cmd.succeeded() && e.cleanup, "a late pass consumes the wait_until");
    check(testing::get_command_error_count() == 0,
          "a wait_until that passes counts no error");
  }

  // wait_until: a stalled (never consumed) check holds past the frame timeout.
  {
    EntityCollection coll;
    Entity &e = coll.createEntity();
    auto &cmd = e.addComponent<PendingE2ECommand>();
    cmd.name = "expect_thing";
    cmd.wait_until_seconds = cmd.retry_budget = 2.f;
    testing::E2ECommandCleanupSystem cleanup;
    for (int i = 0; i < PendingE2ECommand::MAX_FRAMES * 3; i++) {
      cmd.retry_with("thing is 1, expected 2");
      cleanup.for_each_with(e, cmd, 1.f / 60.f);
    }
    check(!cmd.is_consumed(), "a stalled wait_until outlives the frame timeout");
  }

  // wait_until: the budget runs out with the last failure as the error.
  {
    EntityCollection coll;
    Entity &e = coll.createEntity();
    auto &cmd = e.addComponent<PendingE2ECommand>();
    cmd.name = "expect_rope_state";
    cmd.wait_until_seconds = cmd.retry_budget = 0.5f;
    testing::reset_command_error_count();
    testing::E2ECommandCleanupSystem cleanup;
    for (int i = 0; i < 60 && !e.cleanup; i++) {
      cmd.fail("rope is Holding, expected Measuring");
      cleanup.for_each_with(e, cmd, 1.f / 60.f);
    }
    check(e.cleanup && !cmd.succeeded(), "an expired wait_until fails");
    check(cmd.error_message ==
              "wait_until 0.5s: rope is Holding, expected Measuring",
          "the expired error names the wait and the last failure");
    check(testing::get_command_error_count() == 1,
          "an expired wait_until counts one error");
  }

  // mouse_down keeps its press edge one frame for systems that ran before
  // the handler, and the button stays held after that.
  {
    EntityCollection coll;
    Entity &e = coll.createEntity();
    auto &cmd = e.addComponent<PendingE2ECommand>();
    cmd.name = "mouse_down";
    cmd.args = {"10", "20"};
    testing::test_input::reset_all();
    testing::HandleMouseDownCommand handler;
    handler.for_each_with(e, cmd, 0.f);
    auto &m = testing::input_injector::detail::mouse;
    testing::test_input::reset_frame();
    check(m.just_pressed && m.left_down,
          "an unread mouse_down press survives one frame reset");
    testing::test_input::reset_frame();
    check(!m.just_pressed && m.left_down,
          "mouse_down stays held without re-raising the edge");
    testing::test_input::reset_all();
  }

  // parse_script peels wait_until off and parses the rest by its own rules.
  {
    const char *path = "/tmp/afterhours_wait_until_test.e2e";
    FILE *f = fopen(path, "w");
    fputs("wait_until 2.5 expect_text Hello world\n"
          "wait_until 1 my_check a b\n"
          "wait_until nope my_check\n"
          "wait_until 3\n",
          f);
    fclose(f);
    auto cmds = testing::parse_script(path);
    check(cmds.size() == 4, "every wait_until line parses to a command");
    check(cmds[0].name == "expect_text" && cmds[0].wait_until_seconds == 2.5f &&
              cmds[0].args.size() == 1 && cmds[0].args[0] == "Hello world",
          "the inner command keeps its own rest-of-line parse");
    check(cmds[1].name == "my_check" && cmds[1].wait_until_seconds == 1.f &&
              cmds[1].args.size() == 2,
          "an unknown inner command splits its args");
    check(!cmds[2].parse_error.empty(), "a bad timeout is a parse error");
    check(!cmds[3].parse_error.empty(), "a missing command is a parse error");
    remove(path);
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
