// bake — turn a model into house_of_cards impostor cards.
//
//   bake <model> <out_prefix> [--grid N] [--tile px]
//
// Writes <out_prefix>.png (colour), <out_prefix>_depth.png and
// <out_prefix>.cards. Defaults: 16x16 views of 64px.

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace raylib {
#include <raylib.h>
} // namespace raylib

#include <afterhours/ah.h>
#include <afterhours/src/plugins/house_of_cards.h>

using namespace afterhours;

int main(int argc, char **argv) {
  if (argc < 3) {
    std::fprintf(stderr,
                 "usage: bake <model> <out_prefix> [--grid N] [--tile px]\n");
    return 2;
  }
  int grid = 16, tile = 64;
  for (int i = 3; i < argc; i++) {
    if (!std::strcmp(argv[i], "--grid") && i + 1 < argc)
      grid = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "--tile") && i + 1 < argc)
      tile = std::atoi(argv[++i]);
  }

  raylib::SetTraceLogLevel(raylib::LOG_WARNING);
  raylib::SetConfigFlags(raylib::FLAG_WINDOW_HIDDEN);
  raylib::InitWindow(64, 64, "house_of_cards bake");

  raylib::Model model = raylib::LoadModel(argv[1]);
  if (model.meshCount == 0) {
    std::fprintf(stderr, "bake: failed to load %s\n", argv[1]);
    raylib::CloseWindow();
    return 1;
  }
  house_of_cards::Baked baked = house_of_cards::bake(model, grid, tile);
  bool ok = house_of_cards::save(baked, argv[2]);
  if (ok)
    std::printf("bake: %s -> %s (%d views, %dx%d atlas)\n", argv[1], argv[2],
                baked.meta.views(), baked.color.width, baked.color.height);
  else
    std::fprintf(stderr, "bake: failed to write %s.*\n", argv[2]);

  house_of_cards::unload(baked);
  raylib::UnloadModel(model);
  raylib::CloseWindow();
  return ok ? 0 : 1;
}
