// house_of_cards_test.cpp
// Bakes two example models, orbits each against its cards, and checks the
// silhouette overlap. A broken bake, a shader that fails to
// compile (raylib falls back to its default and draws flat squares) or a
// mismatch between the C++ and GLSL view directions all drop the overlap
// well below these floors.
//
// Lit: the cards' baked normals, lit by the same function as the model, must
// land close to the lit model's colours, and lighting from the opposite side
// must not.
//
// Floors sit a little under what was measured (noted per case, and printed
// on every run), so they catch breakage, not driver noise.

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <afterhours/ah.h>
#include <afterhours/tools/house_of_cards/compare.h>

using namespace afterhours;
namespace hoc = afterhours::house_of_cards;

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

struct Result {
  float mean_iou, worst_iou, color_err;
};

static Result run(const raylib::Model &model, hoc::Cards &cards,
                  raylib::Shader shader, const char *label,
                  const hoc::Orbit &lit = {}) {
  hoc::Orbit orbit = lit;
  orbit.angles = 8;
  orbit.size = 128;
  auto scores = hoc::compare(model, cards, shader, orbit);
  hoc::Score m = hoc::mean(scores), w = hoc::worst(scores);
  printf("  %-22s mean_iou %.3f  worst %.3f  color_err %.3f\n", label,
         static_cast<double>(m.iou), static_cast<double>(w.iou),
         static_cast<double>(m.color_err));
  return {m.iou, w.iou, m.color_err};
}

int main() {
  raylib::SetTraceLogLevel(raylib::LOG_ERROR);
  raylib::SetConfigFlags(raylib::FLAG_WINDOW_HIDDEN);
  raylib::InitWindow(64, 64, "house_of_cards_test");
  raylib::Shader shader = hoc::load_shader();
  raylib::Shader lit_cards = hoc::load_shader(DEFAULT_LIGHTING);
  raylib::Shader lit_model = load_lit_shader();
  check(lit_cards.locs[raylib::SHADER_LOC_MAP_NORMAL] != -1,
        "lit card shader compiles");

  Lighting light;
  Lighting opposite = light;
  opposite.direction = {-light.direction.x, -light.direction.y,
                        -light.direction.z};
  hoc::Orbit lit;
  lit.lighting = &light;
  lit.model_shader = lit_model;

  struct Case {
    const char *path;
    float mean, worst;
    float lit_err; // lit colour error ceiling
  } cases[] = {
      // Measured: 0.968 mean, 0.965 worst angle, 0.008 lit colour error.
      {AFTERHOURS_MODELS_DIR "/tree-large.glb", 0.93f, 0.90f, 0.02f},
      // Thin and seen edge-on, the hardest of the examples. Measured: 0.848
      // mean, 0.820 worst angle, 0.045 lit colour error (flat-shaded faces
      // soften where views blend).
      {AFTERHOURS_MODELS_DIR "/kaykit/arch_gate.gltf", 0.80f, 0.75f, 0.07f},
  };

  for (const Case &c : cases) {
    raylib::Model model = raylib::LoadModel(c.path);
    check(model.meshCount > 0, std::string("loads ") + c.path);
    if (model.meshCount == 0) continue;
    printf("%s\n", c.path);

    hoc::Baked baked = hoc::bake(model);
    hoc::Cards cards = hoc::upload(baked);
    Result o = run(model, cards, shader, "baked");
    check(o.mean_iou >= c.mean, "mean overlap");
    check(o.worst_iou >= c.worst, "worst-angle overlap");

    // Flat cards (no depth refinement) must score worse: the depth is doing
    // something.
    hoc::Orbit flat;
    flat.angles = 8;
    flat.size = 128;
    flat.parallax_steps = 0;
    float flat_iou = hoc::mean(hoc::compare(model, cards, shader, flat)).iou;
    printf("  %-22s mean_iou %.3f\n", "flat (no depth)",
           static_cast<double>(flat_iou));
    check(o.mean_iou > flat_iou, "depth refinement beats flat cards");

    // Lit from two opposite sides: the cards must follow the model both
    // times (wrong normals miss on at least one side), and must actually
    // change between them (lighting not ignored).
    std::vector<raylib::Image> shots[2];
    for (int side = 0; side < 2; side++) {
      hoc::Orbit o2 = lit;
      o2.angles = 8;
      o2.size = 128;
      o2.lighting = side ? &opposite : &light;
      auto scores = hoc::compare(model, cards, lit_cards, o2, nullptr,
                                 &shots[side]);
      hoc::Score m = hoc::mean(scores);
      printf("  %-22s mean_iou %.3f  color_err %.3f\n",
             side ? "lit (opposite)" : "lit",
             static_cast<double>(m.iou), static_cast<double>(m.color_err));
      check(m.iou > o.mean_iou - 0.01f, "lighting keeps the silhouette");
      check(m.color_err <= c.lit_err, "lit colours match the lit model");
    }
    float change = 0.f;
    for (size_t i = 0; i < shots[0].size(); i++) {
      change += hoc::detail::score_pair(shots[0][i], shots[1][i], 8).color_err;
      raylib::UnloadImage(shots[0][i]);
      raylib::UnloadImage(shots[1][i]);
    }
    change /= static_cast<float>(shots[0].size());
    printf("  %-22s color change %.3f\n", "light flipped",
           static_cast<double>(change));
    check(change > 0.05f, "moving the light changes the cards");

    // What the bake tool writes must load back and draw the same.
    std::string prefix =
        (std::filesystem::temp_directory_path() / "hoc_test").string();
    check(hoc::save(baked, prefix), "save writes the bake");
    hoc::Cards loaded;
    check(hoc::load(prefix, loaded), "load reads it back");
    if (loaded.color.id != 0) {
      Result l = run(model, loaded, shader, "reloaded");
      check(l.mean_iou > o.mean_iou - 0.01f && l.mean_iou < o.mean_iou + 0.01f,
            "reloaded bake scores the same");
      Result ll = run(model, loaded, lit_cards, "reloaded lit", lit);
      check(ll.color_err <= c.lit_err, "reloaded bake keeps its normals");
      hoc::unload(loaded);
    }

    hoc::unload(cards);
    hoc::unload(baked);
    raylib::UnloadModel(model);
  }

  hoc::Cards missing;
  check(!hoc::load("does/not/exist", missing), "missing bake fails to load");

  raylib::UnloadShader(shader);
  raylib::UnloadShader(lit_cards);
  raylib::UnloadShader(lit_model);
  raylib::CloseWindow();

  printf("house_of_cards_test: %d/%d checks passed\n", checks_passed,
         checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
