#include <cassert>
#include <cstdio>
#include <string>

#include "../../../../src/graphics_common.h"

int main() {
  afterhours::graphics::RunConfig config;
  config.title = "built with -Werror=pedantic";
  std::printf("%s\n", config.title);
  assert(std::string(config.title) == "built with -Werror=pedantic");
  assert(config.width > 0 && config.height > 0);
  return 0;
}
