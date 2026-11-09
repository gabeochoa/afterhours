// compare — orbit a model and its baked cards side by side and score them.
//
//   compare <model> <prefix> [<prefix> ...] [--angles N] [--size px]
//           [--elevation deg] [--distance radii] [--steps N]
//           [--tolerance 0-255] [--strip out.png] [--diff out.png]
//           [--min-iou x]
//
// Prints, per bake: silhouette IoU (mean and worst angle), colour error, and
// the strict pixel test from compare.h (bad pixels per frame, how many of
// those are away from the silhouette edges, their share of the object, the
// largest channel difference). --strip writes one row of
// renders per input (the model first, then each bake), one column per angle;
// --diff writes the matching heatmaps, red where a pixel fails. --min-iou
// makes the exit code fail if any bake's mean IoU is below it.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace raylib {
#include <raylib.h>
} // namespace raylib

#include <afterhours/ah.h>
#include "compare.h"

using namespace afterhours;

int main(int argc, char **argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: compare <model> <prefix> [<prefix> ...] "
                         "[--angles N] [--size px] [--elevation deg] "
                         "[--distance radii] [--steps N] [--tolerance n] "
                         "[--strip out.png] [--diff out.png] [--min-iou x]\n");
    return 2;
  }
  house_of_cards::Orbit orbit;
  std::vector<std::string> prefixes;
  const char *strip_path = nullptr;
  const char *diff_path = nullptr;
  float min_iou = 0.f;
  for (int i = 2; i < argc; i++) {
    auto next = [&] { return i + 1 < argc ? argv[++i] : argv[i]; };
    if (!std::strcmp(argv[i], "--angles")) orbit.angles = std::atoi(next());
    else if (!std::strcmp(argv[i], "--size")) orbit.size = std::atoi(next());
    else if (!std::strcmp(argv[i], "--elevation"))
      orbit.elevation_deg = static_cast<float>(std::atof(next()));
    else if (!std::strcmp(argv[i], "--distance"))
      orbit.distance = static_cast<float>(std::atof(next()));
    else if (!std::strcmp(argv[i], "--steps"))
      orbit.parallax_steps = std::atoi(next());
    else if (!std::strcmp(argv[i], "--tolerance"))
      orbit.tolerance = std::atoi(next());
    else if (!std::strcmp(argv[i], "--diff")) diff_path = next();
    else if (!std::strcmp(argv[i], "--strip")) strip_path = next();
    else if (!std::strcmp(argv[i], "--min-iou"))
      min_iou = static_cast<float>(std::atof(next()));
    else prefixes.push_back(argv[i]);
  }

  raylib::SetTraceLogLevel(raylib::LOG_WARNING);
  raylib::SetConfigFlags(raylib::FLAG_WINDOW_HIDDEN);
  raylib::InitWindow(64, 64, "house_of_cards compare");

  raylib::Model model = raylib::LoadModel(argv[1]);
  if (model.meshCount == 0) {
    std::fprintf(stderr, "compare: failed to load %s\n", argv[1]);
    raylib::CloseWindow();
    return 1;
  }
  raylib::Shader shader = house_of_cards::load_shader();

  int rows = 1 + static_cast<int>(prefixes.size());
  raylib::Image strip = raylib::GenImageColor(orbit.angles * orbit.size,
                                              rows * orbit.size,
                                              {40, 40, 40, 255});
  raylib::Image diffs = raylib::GenImageColor(
      orbit.angles * orbit.size, (rows - 1) * orbit.size, raylib::BLACK);
  auto paste = [&](raylib::Image &into, std::vector<raylib::Image> &shots,
                   int row) {
    for (size_t c = 0; c < shots.size(); c++) {
      raylib::ImageDraw(
          &into, shots[c],
          {0, 0, static_cast<float>(orbit.size), static_cast<float>(orbit.size)},
          {static_cast<float>(static_cast<int>(c) * orbit.size),
           static_cast<float>(row * orbit.size), static_cast<float>(orbit.size),
           static_cast<float>(orbit.size)},
          raylib::WHITE);
      raylib::UnloadImage(shots[c]);
    }
    shots.clear();
  };

  bool ok = true;
  std::printf("distance %.1f radii, %dpx, tolerance %d/255\n",
              static_cast<double>(orbit.distance), orbit.size,
              orbit.tolerance);
  std::printf("%-24s %8s %7s %9s %8s %9s %8s %8s\n", "bake", "mean_iou",
              "worst", "color_err", "bad_px", "bad_inner", "bad_frac",
              "max_diff");
  for (size_t p = 0; p < prefixes.size(); p++) {
    house_of_cards::Cards cards;
    if (!house_of_cards::load(prefixes[p], cards)) {
      ok = false;
      continue;
    }
    std::vector<raylib::Image> model_shots, card_shots;
    auto scores = house_of_cards::compare(model, cards, shader, orbit,
                                          &model_shots, &card_shots);
    std::vector<raylib::Image> heat;
    for (size_t c = 0; c < card_shots.size(); c++)
      heat.push_back(house_of_cards::diff_image(model_shots[c], card_shots[c],
                                                orbit.tolerance));
    paste(diffs, heat, static_cast<int>(p));
    if (p == 0) paste(strip, model_shots, 0);
    else for (raylib::Image &img : model_shots) raylib::UnloadImage(img);
    paste(strip, card_shots, static_cast<int>(p) + 1);

    house_of_cards::Score m = house_of_cards::mean(scores);
    house_of_cards::Score w = house_of_cards::worst(scores);
    std::string name = prefixes[p].substr(prefixes[p].find_last_of('/') + 1);
    std::printf("%-24s %8.3f %7.3f %9.3f %8.0f %9.0f %8.3f %8d\n",
                name.c_str(), static_cast<double>(m.iou),
                static_cast<double>(w.iou), static_cast<double>(m.color_err),
                static_cast<double>(m.bad_px), static_cast<double>(m.bad_inner),
                static_cast<double>(m.bad_frac), m.max_diff);
    if (m.iou < min_iou) ok = false;
    house_of_cards::unload(cards);
  }

  if (strip_path) raylib::ExportImage(strip, strip_path);
  if (diff_path) raylib::ExportImage(diffs, diff_path);
  raylib::UnloadImage(strip);
  raylib::UnloadImage(diffs);
  raylib::UnloadShader(shader);
  raylib::UnloadModel(model);
  raylib::CloseWindow();
  return ok ? 0 : 1;
}
