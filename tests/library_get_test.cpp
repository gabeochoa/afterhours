// Regression test for Library<T>::get() graceful degradation on a missing key.
//
// Before: get() on an unloaded name logged a warning but then fell through to
// storage.at(name), which throws std::out_of_range (a hard abort under
// -fno-exceptions, which consumer games build with). This test locks in that a
// miss now returns a default-constructed fallback instead of throwing/aborting,
// and that the non-const overload resets the fallback each miss so a caller
// can't corrupt the shared fallback for later misses.

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

// Library<T> is abstract (pure-virtual unload + convert_filename_to_object);
// minimal concrete subclass.
template <typename T> struct TestLibrary : Library<T> {
  void unload(T) override {}
  T convert_filename_to_object(const char *, const char *) override {
    return T{};
  }
};

int main() {
  printf("=== Library<T>::get() missing-key degradation ===\n\n");

  // int payload: default-constructs to 0, easy to assert on.
  TestLibrary<int> lib;
  lib.add("real", 42);

  // 1. hit path returns the stored value
  check(lib.get("real") == 42, "get(existing) returns the stored value (42)");

  // 2. MISS path returns a default-constructed fallback instead of throwing.
  //    (Before the fix this line threw std::out_of_range / aborted.)
  int &miss = lib.get("does_not_exist");
  check(miss == 0, "get(missing) returns default-constructed fallback (0), no throw");

  // 3. non-const overload resets the fallback each miss: mutating the returned
  //    fallback must not leak into a subsequent miss.
  miss = 999;
  int &miss2 = lib.get("also_missing");
  check(miss2 == 0, "fallback is reset per miss (prior miss mutation does not leak)");

  // 4. const overload miss also degrades (returns fallback, no throw)
  const TestLibrary<int> &clib = lib;
  check(clib.get("nope") == 0, "const get(missing) returns fallback, no throw");

  // 5. a miss did not corrupt real entries
  check(lib.get("real") == 42, "hit still correct after misses");

  printf("\n%d/%d checks passed\n", tests_passed, tests_run);
  if (tests_passed != tests_run) { printf("FAILURES: %d\n", tests_run - tests_passed); return 1; }
  printf("All checks passed!\n");
  return 0;
}
