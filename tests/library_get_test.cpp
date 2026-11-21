#include <afterhours/src/library.h>

#include <cstdio>
#include <string>

static int tests_run = 0;
static int tests_passed = 0;
static void check(bool cond, const char *what) {
  tests_run++;
  if (cond) {
    tests_passed++;
    printf("  PASS: %s\n", what);
  } else {
    printf("  FAIL: %s\n", what);
  }
}

template <typename T> struct TestLibrary : Library<T> {
  void unload(T) override {}
  T convert_filename_to_object(const char *, const char *) override {
    return T{};
  }
};

int main() {
  printf("=== Library<T>::get() missing-key degradation ===\n\n");

  TestLibrary<int> lib;
  lib.add("real", 42);

  check(lib.get("real") == 42, "get(existing) returns the stored value (42)");

  int &miss = lib.get("does_not_exist");
  check(miss == 0, "get(missing) returns default-constructed fallback (0), no throw");

  miss = 999;
  int &miss2 = lib.get("also_missing");
  check(miss2 == 0, "fallback is reset per miss (prior miss mutation does not leak)");

  const TestLibrary<int> &clib = lib;
  check(clib.get("nope") == 0, "const get(missing) returns fallback, no throw");

  check(lib.get("real") == 42, "hit still correct after misses");

  printf("\n%d/%d checks passed\n", tests_passed, tests_run);
  if (tests_passed != tests_run) { printf("FAILURES: %d\n", tests_run - tests_passed); return 1; }
  printf("All checks passed!\n");
  return 0;
}
