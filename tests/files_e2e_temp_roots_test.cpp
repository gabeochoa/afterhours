#include <afterhours/src/plugins/files.h>

#include <cstdio>
#include <cstdlib>
#include <random>
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

static fs::path fresh_dir(const char *prefix) {
  std::random_device rd;
  fs::path dir = fs::temp_directory_path() /
                 (std::string(prefix) + std::to_string(rd()) + "-" +
                  std::to_string(rd()));
  fs::create_directories(dir);
  return dir;
}

int main() {
  const fs::path job_tmp = fresh_dir("afterhours-e2e-tmp-");
  setenv("E2E_TMPDIR", job_tmp.c_str(), 1);

  {
    char *argv[] = {const_cast<char *>("game"), nullptr};
    check(!files::maybe_install_e2e_test_roots(1, argv),
          "production argv installs nothing");
    check(files::active_test_roots == nullptr, "production leaves roots off");
  }

  setenv("AFTERHOURS_NO_E2E_TEMPROOTS", "1", 1);
  {
    char *argv[] = {const_cast<char *>("game"),
                    const_cast<char *>("--test-script"),
                    const_cast<char *>("x.e2e"), nullptr};
    check(!files::maybe_install_e2e_test_roots(3, argv),
          "opt-out env disables the install");
    check(files::active_test_roots == nullptr, "opt-out leaves roots off");
  }
  unsetenv("AFTERHOURS_NO_E2E_TEMPROOTS");

  {
    char *argv[] = {const_cast<char *>("game"),
                    const_cast<char *>("--test-script=x.e2e"), nullptr};
    check(files::maybe_install_e2e_test_roots(2, argv),
          "e2e argv installs roots");
    check(files::active_test_roots != nullptr, "roots are active");
    const fs::path root = files::active_test_roots->root;
    check(root.parent_path() == job_tmp, "root sits under E2E_TMPDIR");
    check(!files::maybe_install_e2e_test_roots(2, argv),
          "second install is a no-op");

    files::init("afterhours-e2e-temp-roots");
    check(files::get_save_path() == files::active_test_roots->save,
          "init lands in the temp save dir");
    check(files::get_config_path() == files::active_test_roots->config,
          "init lands in the temp config dir");
    check(files::write_string_atomic(files::get_save_path() / "settings.json",
                                     R"({"volume": 0.5})"),
          "write into the temp save path");
    check(files::read_string(files::get_save_path() / "settings.json") ==
              R"({"volume": 0.5})",
          "read back the settings");
    check(files::get_save_path().filename() == "save",
          "save path is the temp layout, not the real game dir");

    files::TestRoots second_run;
    check(second_run.root != root, "second run gets its own root");
    check(!fs::exists(second_run.save / "settings.json"),
          "second run cannot see the first run's settings");
  }

  std::error_code ec;
  fs::remove_all(job_tmp, ec);
  if (failures == 0)
    std::printf("files_e2e_temp_roots_test: all checks passed\n");
  return failures ? 1 : 0;
}
