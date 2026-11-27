// default_logger_test.cpp
// Default logger: runtime minimum level, log_once_per throttling, and the
// raylib TraceLog route through log_sink_fn.

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include <afterhours/ah.h>
#include <afterhours/src/graphics.h>
#include <afterhours/src/logging.h>

using namespace afterhours;

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

static std::vector<std::string> captured;

static void capture_sink(const char *level, const char *message) {
  captured.emplace_back(std::string(level) + " " + message);
}

static bool captured_contains(const std::string &needle) {
  for (const auto &line : captured)
    if (line.find(needle) != std::string::npos)
      return true;
  return false;
}

static int captured_count(const std::string &needle) {
  int count = 0;
  for (const auto &line : captured)
    if (line.find(needle) != std::string::npos)
      count++;
  return count;
}

int main() {
  log_sink_fn = &capture_sink;
  raylib::SetTraceLogLevel(raylib::LOG_ALL);

  set_log_level(LogLevel::LOG_WARN);
  captured.clear();
  log_info("below the minimum");
  check(!captured_contains("below the minimum"),
        "set_log_level filters out lower levels");
  log_error("above the minimum");
  check(captured_contains("above the minimum"),
        "levels at or above the minimum still emit");
  set_log_level(LogLevel::LOG_TRACE);
  captured.clear();
  log_info("back at trace minimum");
  check(captured_contains("back at trace minimum"),
        "resetting to LOG_TRACE restores info logging");

  captured.clear();
  log_once_per(std::chrono::milliseconds(50), 7101, "once per first");
  log_once_per(std::chrono::milliseconds(50), 7101, "once per first");
  check(captured_count("once per first") == 1,
        "log_once_per emits once inside the interval");
  std::this_thread::sleep_for(std::chrono::milliseconds(60));
  log_once_per(std::chrono::milliseconds(50), 7101, "once per first");
  check(captured_count("once per first") == 2,
        "log_once_per emits again after the interval elapses");

  captured.clear();
  graphics::route_backend_logs(LogLevel::LOG_INFO);
  raylib::TraceLog(raylib::LOG_INFO, "routed %s", "hello");
  check(captured_contains("routed hello"),
        "route_backend_logs forwards TraceLog through the sink");
  raylib::TraceLog(raylib::LOG_TRACE, "too quiet to route");
  check(!captured_contains("too quiet to route"),
        "the route drops entries below its minimum level");

  captured.clear();
  graphics::route_backend_logs(
      LogLevel::LOG_TRACE,
      [](LogLevel, std::string_view message) {
        return message.find("secret") == std::string_view::npos;
      });
  raylib::TraceLog(raylib::LOG_INFO, "a secret routed line");
  check(!captured_contains("secret routed"),
        "the route filter can reject a message");
  raylib::TraceLog(raylib::LOG_INFO, "a public routed line");
  check(captured_contains("public routed"),
        "the route filter lets other messages through");
  graphics::clear_backend_log_route();

  set_log_level(LogLevel::LOG_TRACE);
  log_sink_fn = nullptr;
  printf("default_logger_test: %d/%d checks passed\n", checks_passed,
         checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
