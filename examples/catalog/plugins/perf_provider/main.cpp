#include <cstdio>
#include <optional>

#include "../../../../ah.h"
#include "../../../../src/plugins/e2e_testing/perf_commands.h"

using namespace afterhours::testing::perf_commands;

int main() {
  PerfProvider custom;
  custom.get_fps = []() -> std::optional<float> { return 60.f; };
  custom.top_entries = [](int) {
    return std::vector<PerfEntry>{{"custom_system", 1.5f, std::nullopt, 3}};
  };
  set_provider(custom);
  std::printf("custom fps: %.1f\n", provider().get_fps().value_or(0.f));
  for (const auto &e : provider().top_entries(10))
    std::printf("  %.2fms %s\n", static_cast<double>(e.ms), e.name.c_str());

  builtin_profile::enable();
  std::printf("builtin provider installed: %s\n",
              provider().top_entries ? "yes" : "no");
  builtin_profile::disable();
  return 0;
}
