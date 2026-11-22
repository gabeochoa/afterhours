#include <afterhours/src/plugins/e2e_testing/script_headers.h>

#include <cstdio>
#include <string>
#include <vector>

using namespace afterhours::testing;

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
  printf("=== e2e script headers ===\n\n");

  {
    const auto h = parse_script_headers_text(
        "# timeout: 30\n# flags: --headless --seed=1\n"
        "# requires: gpu, file:assets.dat env:STEAM_SDK\n"
        "# batch: gpu-heavy\n"
        "# spawn: server ./game --serve --port={port}\n"
        "# wait_for_log: server:READY\n"
        "wait_frames 1\n");
    check(h.errors.empty(), "a full header block parses clean");
    check(h.timeout_seconds && *h.timeout_seconds == 30.f,
          "timeout header parses");
    check(h.flags == std::vector<std::string>({"--headless", "--seed=1"}),
          "flags header shell-splits");
    check(h.requirements ==
              std::vector<std::string>({"gpu", "file:assets.dat", "env:STEAM_SDK"}),
          "requires splits on commas and whitespace");
    check(h.batch == "gpu-heavy", "batch header parses");
    check(h.spawns.size() == 1 && h.spawns[0].label == "server" &&
              h.spawns[0].argv.size() == 3,
          "spawn header keeps label and argv");
    check(h.wait_for_logs.size() == 1 && h.wait_for_logs[0].label == "server" &&
              h.wait_for_logs[0].needle == "READY",
          "wait_for_log parses label:needle");
  }

  {
    const auto h = parse_script_headers_text("# timeout: 2.5s\nwait 1\n");
    check(h.timeout_seconds && *h.timeout_seconds == 2.5f,
          "timeout accepts a trailing s");
  }
  {
    const auto h = parse_script_headers_text("# timeout: soon\nwait 1\n");
    check(!h.timeout_seconds && h.errors.size() == 1,
          "a non-numeric timeout is an error, not a silent default");
  }
  {
    const auto h = parse_script_headers_text("# timeout: -3\nwait 1\n");
    check(!h.timeout_seconds && h.errors.size() == 1,
          "a negative timeout is an error");
  }

  {
    const auto h = parse_script_headers_text(
        "wait_frames 1\n# timeout: 99\n");
    check(!h.timeout_seconds, "headers after the first command are comments");
  }
  {
    const auto h = parse_script_headers_text(
        "# an ordinary comment\n# unknown: thing\nwait_frames 1\n");
    check(h.errors.empty() && !h.timeout_seconds,
          "ordinary comments and unknown headers are ignored");
  }

  {
    const auto h = parse_script_headers_text(
        "# flags: --name \"two words\" --empty ''\nwait 1\n");
    check(h.flags == std::vector<std::string>({"--name", "two words", "--empty", ""}),
          "quoted flags stay one word");
  }
  {
    const auto h = parse_script_headers_text("# flags: --name \"oops\nwait 1\n");
    check(h.errors.size() == 1, "an unterminated quote is an error");
  }
  {
    const auto h = parse_script_headers_text("# spawn: lonely\nwait 1\n");
    check(h.errors.size() == 1 && h.spawns.empty(),
          "a spawn with no command is an error");
  }
  {
    const auto h = parse_script_headers_text(
        "# spawn: srv ./srv\n# wait_for_log: listening on: 8080\nwait 1\n");
    check(h.wait_for_logs.size() == 1 && h.wait_for_logs[0].label.empty() &&
              h.wait_for_logs[0].needle == "listening on: 8080",
          "a needle with spaces is not mistaken for a label");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
