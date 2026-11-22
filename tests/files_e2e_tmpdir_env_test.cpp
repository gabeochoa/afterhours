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

int main() {
  std::random_device rd;
  const fs::path job_tmp = fs::temp_directory_path() /
                           ("afterhours-e2e-env-" + std::to_string(rd()) + "-" +
                            std::to_string(rd()));
  fs::create_directories(job_tmp);
  setenv("E2E_TMPDIR", job_tmp.c_str(), 1);

  check(files::active_test_roots == nullptr, "no explicit roots in this run");
  files::init("afterhours-e2e-tmpdir-env");
  check(files::get_save_path() == job_tmp / "save",
        "save path follows E2E_TMPDIR");
  check(files::get_config_path() == job_tmp / "config",
        "config path follows E2E_TMPDIR");
  check(files::write_string_atomic(files::get_save_path() / "slot.json", "{}"),
        "write into the isolated save path");
  check(fs::exists(job_tmp / "save" / "slot.json"), "slot landed in the job dir");

  std::error_code ec;
  fs::remove_all(job_tmp, ec);
  if (failures == 0)
    std::printf("files_e2e_tmpdir_env_test: all checks passed\n");
  return failures ? 1 : 0;
}
