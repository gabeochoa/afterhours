#include <cstdio>

#include "../../../../src/graphics_common.h"

int main() {
  afterhours::graphics::RunConfig config;
  config.title = "built with -Werror=pedantic";
  std::printf("%s\n", config.title);
  return 0;
}
