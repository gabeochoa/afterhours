// files_isolated_storage_test.cpp
// AFTERHOURS_TEST_STORAGE_DIR and files::isolate_storage put config, saves
// and temp under one test root. Apps were redirecting config and forgetting
// saves, and fixture records landed in the user's real save folder.
//
// HOME (and XDG_*) point at a scratch dir, so the platform-folder lookup this
// test is guarding against can only ever touch that scratch dir.

#include <afterhours/src/plugins/files.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;
using afterhours::files;
using ProvidesResourcePaths = afterhours::files::ProvidesResourcePaths;

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

static bool under(const fs::path &p, const fs::path &root) {
  const auto rel = fs::weakly_canonical(p).lexically_relative(
      fs::weakly_canonical(root));
  return !rel.empty() && *rel.begin() != "..";
}

int main() {
  printf("=== files isolated storage ===\n\n");
  std::error_code ec;
  const fs::path scratch = fs::temp_directory_path() /
                           ("afh_storage_" + std::to_string(getpid()));
  fs::remove_all(scratch, ec);
  const fs::path home = scratch / "home";
  const fs::path root = scratch / "test-root";
  fs::create_directories(home);
  setenv("HOME", home.c_str(), 1);
  for (const char *xdg : {"XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_STATE_HOME",
                          "XDG_CACHE_HOME"})
    unsetenv(xdg);

  // Control: without isolation the constructor creates the platform save
  // folder, which is what the isolated case must not do.
  unsetenv(files::TEST_STORAGE_ENV);
  {
    ProvidesResourcePaths real("isogame", "resources");
    check(under(real.save_folder_path, home),
          "control: platform save folder resolves under HOME");
    check(fs::exists(real.save_folder_path),
          "control: and the constructor creates it");
    check(!real.temp_folder_path.empty(),
          "control: temp has a default outside the test root");
  }
  fs::remove_all(home, ec);
  fs::create_directories(home);

  setenv(files::TEST_STORAGE_ENV, root.c_str(), 1);
  {
    ProvidesResourcePaths p("isogame", "resources");
    check(p.config_folder_path == root / "config" / "isogame",
          "config goes to <root>/config/<game>");
    check(p.save_folder_path == root / "save" / "isogame",
          "saves go to <root>/save/<game>");
    check(p.temp_folder_path == root / "tmp" / "isogame",
          "temp goes to <root>/tmp/<game>");
    for (const fs::path &d :
         {p.config_folder_path, p.save_folder_path, p.temp_folder_path})
      check(fs::is_directory(d), "created " + d.string());
    check(fs::is_empty(home),
          "nothing under HOME was created, so real folders are untouched");
  }
  unsetenv(files::TEST_STORAGE_ENV);

  // After init: the host learned its test dir from argv.
  {
    afterhours::EntityCollection coll;
    afterhours::EntityHelper::set_default_collection(&coll);
    const fs::path late = scratch / "late-root";
    check(!files::isolate_storage(late),
          "isolate_storage reports failure before init");
    files::init("lategame");
    check(files::isolate_storage(late), "isolate_storage succeeds after init");
    check(files::get_config_path() == late / "config" / "lategame",
          "config redirected");
    check(files::get_save_path() == late / "save" / "lategame",
          "saves redirected with it");
    check(files::get_temp_path() == late / "tmp" / "lategame",
          "temp redirected with it");
    check(files::write_string_atomic(files::get_save_path() / "slot.json",
                                     "{}"),
          "a save lands");
    check(fs::exists(late / "save" / "lategame" / "slot.json"),
          "inside the test root");
    afterhours::EntityHelper::set_default_collection(nullptr);
  }

  fs::remove_all(scratch, ec);
  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
