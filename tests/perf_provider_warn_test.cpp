#include "ui_test_harness.h"
#include <afterhours/src/plugins/e2e_testing/perf_commands.h>

#include <string>
#include <vector>

namespace {
std::vector<std::string> &warnings() {
  static std::vector<std::string> v;
  return v;
}
int count_containing(const std::string &needle) {
  int n = 0;
  for (const auto &w : warnings())
    if (w.find(needle) != std::string::npos) ++n;
  return n;
}
int checks_run = 0;
int checks_passed = 0;
void check(bool cond, const char *what) {
  ++checks_run;
  if (cond) {
    ++checks_passed;
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", what);
  }
}
} // namespace

int main() {
  log_sink_fn = [](const char *level, const char *message) {
    if (std::string(level).find("WARN") != std::string::npos)
      warnings().push_back(message);
  };

  using namespace afterhours::testing::perf_commands;

  provider() = PerfProvider{};
  warnings().clear();
  PerfProvider first;
  first.get_fps = []() -> std::optional<float> { return 60.f; };
  set_provider(first);
  check(count_containing("set_provider") == 0,
        "installing the first provider does not warn");
  check(provider().get_fps && *provider().get_fps() == 60.f,
        "first provider is installed");

  PerfProvider second;
  second.get_fps = []() -> std::optional<float> { return 30.f; };
  set_provider(second);
  check(count_containing("set_provider") == 1,
        "overwriting an existing provider warns once");
  check(provider().get_fps && *provider().get_fps() == 30.f,
        "overwriting provider still takes effect");

  set_provider(second);
  check(count_containing("set_provider") == 1,
        "overwriting again does not warn a second time");

  provider() = PerfProvider{};
  warnings().clear();
  builtin_profile::enable();
  check(count_containing("builtin_profile::enable") == 1,
        "enable with profiling compiled out warns once");
  check(provider().top_entries != nullptr,
        "enable still installs the builtin provider");
  builtin_profile::enable();
  check(count_containing("builtin_profile::enable") == 1,
        "enable again does not warn a second time");
  builtin_profile::disable();
  provider() = PerfProvider{};
  log_sink_fn = nullptr;

  std::printf("%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
