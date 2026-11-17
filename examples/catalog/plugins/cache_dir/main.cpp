#include <cstdio>
#include <filesystem>
#include <fstream>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_SYSTEM
#include "../../../../ah.h"
#include "../../../../src/plugins/files.h"

using namespace afterhours;

int main(int, char **) {
  files::init("cache_dir_example", "resources");

  const auto cache = files::get_cache_path();
  printf("config: %s\n", files::get_config_path().string().c_str());
  printf("cache:  %s\n", cache.string().c_str());

  if (!files::ensure_directory_exists(cache))
    return 1;
  const auto thumbnail = cache / "thumbnail.bin";
  std::ofstream(thumbnail) << "regenerable data";
  const bool written = std::filesystem::exists(thumbnail);

  std::filesystem::remove_all(cache);
  std::filesystem::remove(files::get_save_path());
  return written && cache != files::get_config_path() ? 0 : 1;
}
