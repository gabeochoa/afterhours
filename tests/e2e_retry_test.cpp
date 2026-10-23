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

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
