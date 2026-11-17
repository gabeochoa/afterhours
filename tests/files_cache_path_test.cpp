#include <afterhours/src/plugins/files.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using ProvidesResourcePaths = afterhours::files::ProvidesResourcePaths;

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond)
    checks_passed++;
  else
    fprintf(stderr, "  FAIL: %s\n", what.c_str());
}

int main() {
#if defined(__linux__)
  setenv("XDG_CACHE_HOME", "/tmp/afh-cache-probe-xdg", 1);
#endif
  ProvidesResourcePaths paths("afh_cache_probe", "resources");
  const fs::path cache = paths.get_cache_path();
  printf("  cache:  %s\n  config: %s\n", cache.string().c_str(),
         paths.get_config_path().string().c_str());

  check(!cache.empty(), "a cache path is set");
  check(cache.filename() == "afh_cache_probe", "it ends in the game's name");
  check(cache != paths.get_config_path(), "it is not the config directory");
  check(cache != paths.get_save_path(), "it is not the save directory");
#if defined(__APPLE__)
  check(cache.string().find("/Library/Caches/") != std::string::npos,
        "macOS puts it under ~/Library/Caches");
#elif defined(__linux__)
  check(cache == fs::path("/tmp/afh-cache-probe-xdg/afh_cache_probe"),
        "Linux honours XDG_CACHE_HOME");
#endif

  std::error_code ec;
  fs::remove(paths.get_save_path(), ec);

  printf("%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
