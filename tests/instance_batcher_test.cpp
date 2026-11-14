// instance_batcher_test.cpp
// InstanceBatcher and model_transform under raylib:
//   - model_transform matches raymath's scale * rotateY * translate, the
//     composition DrawModelEx uses
//   - copies of two models land in two groups, and flush empties them
//   - a batched frame renders the same pixels as one draw_model per copy
//
// Opens a hidden window: models and render targets need a GL context.

#include <cmath>
#include <cstdio>
#include <random>
#include <string>

namespace raylib {
#include <raylib.h>
// raymath zero-initialises with `= { 0 }`, which -Wextra flags once per use.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#include <raymath.h>
#pragma clang diagnostic pop
} // namespace raylib

#include <afterhours/ah.h>
#include <afterhours/src/plugins/models.h>

using namespace afterhours;

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

static float max_abs_diff(const raylib::Matrix &a, const raylib::Matrix &b) {
  const float *pa = &a.m0, *pb = &b.m0;
  float worst = 0.f;
  for (int i = 0; i < 16; i++) worst = std::fmax(worst, std::fabs(pa[i] - pb[i]));
  return worst;
}

int main() {
  // model_transform against raymath.
  {
    std::mt19937 rng(3);
    std::uniform_real_distribution<float> unit(-1.f, 1.f);
    float worst = 0.f;
    for (int i = 0; i < 1000; i++) {
      raylib::Vector3 pos = {unit(rng) * 50, unit(rng) * 50, unit(rng) * 50};
      raylib::Vector3 scale = {1.5f + unit(rng), 1.5f + unit(rng), 1.5f + unit(rng)};
      float yaw = unit(rng) * 720.f;
      raylib::Matrix expected = raylib::MatrixMultiply(
          raylib::MatrixMultiply(raylib::MatrixScale(scale.x, scale.y, scale.z),
                                 raylib::MatrixRotateY(yaw * DEG2RAD)),
          raylib::MatrixTranslate(pos.x, pos.y, pos.z));
      worst = std::fmax(worst, max_abs_diff(model_transform(pos, yaw, scale), expected));
    }
    check(worst < 1e-4f, "model_transform matches raymath");
  }

  raylib::SetTraceLogLevel(raylib::LOG_ERROR);
  raylib::SetConfigFlags(raylib::FLAG_WINDOW_HIDDEN);
  raylib::InitWindow(64, 64, "instance_batcher_test");
  auto &models = ModelLibrary::get();
  check(models.load(AFTERHOURS_MODELS_DIR "/tree-large.glb", "tree"), "tree loads");
  check(models.load(AFTERHOURS_MODELS_DIR "/kaykit/lantern.gltf", "lantern"),
        "lantern loads");
  if (!models.contains("tree") || !models.contains("lantern")) {
    raylib::CloseWindow();
    printf("instance_batcher_test: %d/%d checks passed\n", checks_passed, checks_run);
    return 1;
  }
  const ModelType &tree = models.get("tree");
  const ModelType &lantern = models.get("lantern");

  // Grouping.
  InstanceBatcher batcher;
  raylib::Shader shader = load_instancing_shader();
  struct Copy {
    const ModelType *model;
    raylib::Vector3 pos;
    float yaw;
    float scale;
  } scene[] = {
      {&tree, {-3, 0, 0}, 0.f, 1.f},     {&lantern, {-1, 0, 1}, 30.f, 1.f},
      {&tree, {1, 0, -1}, 90.f, 0.8f},   {&lantern, {3, 0, 0}, 200.f, 1.2f},
      {&tree, {0, 0, 2}, 45.f, 1.1f},
  };
  for (const Copy &c : scene)
    batcher.add(*c.model, c.pos, c.yaw, {c.scale, c.scale, c.scale});
  check(batcher.copies() == 5, "every copy is collected");
  check(batcher.group_count() == 2, "copies group by model");

  // Keys split one model's copies; on_group sees each group once, in
  // first-seen order, before it draws.
  {
    InstanceBatcher keyed;
    keyed.add(tree, {0, 0, 0}, 0.f, {1, 1, 1}, 0);
    keyed.add(tree, {1, 0, 0}, 0.f, {1, 1, 1}, 1);
    keyed.add(lantern, {2, 0, 0}, 0.f, {1, 1, 1}, 1);
    keyed.add(tree, {3, 0, 0}, 0.f, {1, 1, 1}, 1);
    check(keyed.group_count() == 3, "a key splits one model into two groups");
    std::string seen;
    raylib::RenderTexture2D scratch = raylib::LoadRenderTexture(8, 8);
    raylib::BeginTextureMode(scratch);
    keyed.flush(shader, [&](const ModelType &m, uint32_t key,
                            std::span<const MatrixType> ts) {
      seen += std::string(&m == &tree ? "tree" : "lantern") + ":" +
              std::to_string(key) + "x" + std::to_string(ts.size()) + " ";
    });
    raylib::EndTextureMode();
    raylib::UnloadRenderTexture(scratch);
    check(seen == "tree:0x1 tree:1x2 lantern:1x1 ",
          "on_group runs per group with its key and transforms: " + seen);
    check(keyed.copies() == 0, "a flush with on_group empties the groups");
  }

  // Same frame two ways: batched, and one draw_model per copy.
  raylib::Camera3D cam{};
  cam.position = {0, 6, 10};
  cam.target = {0, 1, 0};
  cam.up = {0, 1, 0};
  cam.fovy = 45.f;
  cam.projection = raylib::CAMERA_PERSPECTIVE;
  raylib::RenderTexture2D rt = raylib::LoadRenderTexture(256, 256);
  auto render = [&](bool batched) {
    raylib::BeginTextureMode(rt);
    raylib::ClearBackground({0, 0, 0, 0});
    raylib::BeginMode3D(cam);
    if (batched) {
      batcher.flush(shader);
    } else {
      for (const Copy &c : scene)
        draw_model(*c.model, c.pos, c.yaw, {c.scale, c.scale, c.scale});
    }
    raylib::EndMode3D();
    raylib::EndTextureMode();
    raylib::Image img = raylib::LoadImageFromTexture(rt.texture);
    raylib::ImageFormat(&img, raylib::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    return img;
  };
  raylib::Image batched = render(true);
  check(batcher.copies() == 0 && batcher.group_count() == 0,
        "flush empties the groups");
  raylib::Image direct = render(false);

  auto *pa = static_cast<raylib::Color *>(batched.data);
  auto *pb = static_cast<raylib::Color *>(direct.data);
  int covered = 0, differ = 0;
  for (int i = 0; i < 256 * 256; i++) {
    if (pa[i].a > 0 || pb[i].a > 0) covered++;
    int d = std::abs(pa[i].r - pb[i].r) + std::abs(pa[i].g - pb[i].g) +
            std::abs(pa[i].b - pb[i].b) + std::abs(pa[i].a - pb[i].a);
    if (d > 6) differ++;
  }
  printf("  %d covered pixels, %d differ\n", covered, differ);
  check(covered > 2000, "the scene is on screen");
  check(differ <= covered / 200, "batched frame matches draw_model per copy");

  raylib::UnloadImage(batched);
  raylib::UnloadImage(direct);
  raylib::UnloadRenderTexture(rt);
  raylib::UnloadShader(shader);
  models.unload_all();
  raylib::CloseWindow();

  printf("instance_batcher_test: %d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
