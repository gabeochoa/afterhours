
#define FMT_HEADER_ONLY
#define AFTER_HOURS_USE_NONE
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/library.h>

#include <cstdio>
#include <stdexcept>
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

struct IntLibrary : Library<int> {
  int convert_filename_to_object(const char *, const char *) override {
    return 0;
  }
  void unload(int) override {}
};

int main() {
  IntLibrary lib;
  lib.add("tree", 1);
  lib.add("rock", 2);

  auto tree = lib.intern("tree");
  auto rock = lib.intern("rock");
  check(tree != IntLibrary::invalid_handle, "intern returns a valid handle");
  check(tree != rock, "different names get different handles");
  check(lib.intern("tree") == tree, "same name -> same handle");
  check(lib.intern(std::string("rock")) == rock,
        "same name -> same handle via string");

  check(lib.get(tree) == lib.get("tree"), "handle get equals string get");
  check(lib.get(rock) == lib.get("rock"), "handle get equals string get (2)");
  check(&lib.get(tree) == &lib.get("tree"), "handle and string name one entry");
  check(lib.contains(tree) && lib.contains(rock), "contains(Handle)");

  auto later = lib.intern("later");
  check(!lib.contains(later), "unloaded handle is absent");
  lib.add("later", 3);
  check(lib.contains(later) && lib.get(later) == 3,
        "add() links a pre-interned handle");

  bool threw = false;
  try {
    (void)lib.get(IntLibrary::invalid_handle);
  } catch (const std::out_of_range &) {
    threw = true;
  }
  check(threw, "get(invalid_handle) throws");
  check(!lib.contains(IntLibrary::invalid_handle),
        "invalid handle is not contained");

  lib.unload_all();
  check(!lib.contains(tree), "handle is dropped by unload_all");

  std::printf("library_intern_test: %d/%d checks passed\n", checks - failures,
              checks);
  return failures > 0 ? 1 : 0;
}
