// should_run_overload_test.cpp
// SystemManager calls the non-const should_run. A system that overrides only
// the const overload used to have its gate ignored and ran every frame.

#define FMT_HEADER_ONLY
#define AFTER_HOURS_USE_NONE
#include <fmt/format.h>

#include <afterhours/ah.h>

#include <cstdio>
#include <memory>
#include <string>

using namespace afterhours;

static int checks = 0;
static int failures = 0;

static void check(bool cond, const std::string &what) {
  ++checks;
  if (!cond) {
    ++failures;
    std::printf("  FAIL: %s\n", what.c_str());
  }
}

struct ConstGate : System<> {
  int *runs;
  explicit ConstGate(int &r) : runs(&r) {}
  bool should_run(const float) const override { return false; }
  void once(const float) override { ++*runs; }
};

struct MutableGate : System<> {
  int *runs;
  explicit MutableGate(int &r) : runs(&r) {}
  bool should_run(const float) override { return false; }
  void once(const float) override { ++*runs; }
};

struct NoGate : System<> {
  int *runs;
  explicit NoGate(int &r) : runs(&r) {}
  void once(const float) override { ++*runs; }
};

int main() {
  int const_runs = 0, mutable_runs = 0, open_runs = 0;
  SystemManager sm;
  sm.register_update_system(std::make_unique<ConstGate>(const_runs));
  sm.register_update_system(std::make_unique<MutableGate>(mutable_runs));
  sm.register_update_system(std::make_unique<NoGate>(open_runs));
  for (int i = 0; i < 3; i++) sm.run(1.f / 60.f);

  check(const_runs == 0, "a const-only gate stops the system");
  check(mutable_runs == 0, "a non-const gate still stops the system");
  check(open_runs == 3, "an ungated system runs every frame");

  std::printf("should_run_overload_test: %d/%d checks passed\n",
              checks - failures, checks);
  return failures > 0 ? 1 : 0;
}
