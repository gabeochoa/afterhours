// library_duplicate_test.cpp
// Library::load with a name that is already loaded must not convert the file:
// the converted object used to be dropped by add() without unload, leaking it.

#define FMT_HEADER_ONLY
#define AFTER_HOURS_USE_NONE
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/library.h>

#include <cstdio>
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

struct CountingLibrary : Library<int> {
  int converted = 0;
  int unloaded = 0;
  int convert_filename_to_object(const char *, const char *) override {
    return ++converted;
  }
  void unload(int) override { ++unloaded; }
};

int main() {
  CountingLibrary lib;
  lib.load("a.png", "a");
  lib.load("a_again.png", "a");
  lib.load("b.png", "b");

  check(lib.converted == 2, "a duplicate name is not converted");
  check(lib.size() == 2, "two distinct names are stored");
  check(lib.get("a") == 1, "the first load keeps the name");
  lib.unload_all();
  check(lib.unloaded == lib.converted, "every converted object is unloaded");

  std::printf("library_duplicate_test: %d/%d checks passed\n",
              checks - failures, checks);
  return failures > 0 ? 1 : 0;
}
