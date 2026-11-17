#include <afterhours/src/plugins/files.h>

#include <chrono>
#include <cstdio>
#include <string>

namespace fs = std::filesystem;
using afterhours::files;

static int failures = 0;
static void check(bool ok, const char *what) {
  if (!ok) {
    ++failures;
    std::fprintf(stderr, "FAIL: %s\n", what);
  }
}

static bool inside(const fs::path &path, const fs::path &root) {
  auto rel = path.lexically_relative(root);
  return !rel.empty() && *rel.begin() != "..";
}

int main() {
  const std::string game =
      "afterhours-test-roots-" +
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  fs::path outer_root, inner_root;
  {
    files::TestRoots outer;
    outer_root = outer.root;
    check(fs::is_directory(outer.save) && fs::is_directory(outer.config) &&
              fs::is_directory(outer.temp),
          "save, config and temp exist");
    check(inside(outer.temp, outer.root), "temp is under the root");

    files::init(game);
    check(files::get_save_path() == outer.save, "init under a scope saves there");
    check(files::get_config_path() == outer.config,
          "init under a scope configures there");
    check(files::write_string_atomic(files::get_save_path() / "slot.json", "{}"),
          "write into the scoped save path");

    {
      files::TestRoots inner;
      inner_root = inner.root;
      check(inner.root != outer.root, "nested scopes get distinct roots");
      check(files::get_save_path() == inner.save,
            "an existing provider is redirected");
      check(!fs::exists(inner.save / "slot.json"),
            "the inner scope cannot see the outer scope's files");
    }
    check(!fs::exists(inner_root), "the inner root is deleted");
    check(files::get_save_path() == outer.save,
          "the outer paths come back after the inner scope");
    check(fs::exists(outer.save / "slot.json"),
          "the inner scope left the outer files alone");
  }
  check(!fs::exists(outer_root), "the outer root is deleted");
  const fs::path restored = files::get_save_path();
  check(!restored.empty() && !inside(restored, outer_root),
        "the provider leaves the deleted root");
  check(restored.filename() == game, "the provider is back on the default path");
  check(!fs::exists(restored), "the default save folder was never created");
  check(files::active_test_roots == nullptr, "no scope is left active");

  if (failures == 0)
    std::printf("files_test_roots_test: all checks passed\n");
  return failures ? 1 : 0;
}
