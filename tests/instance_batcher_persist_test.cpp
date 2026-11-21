// instance_batcher_persist_test.cpp
// InstanceBatcher's GPU instance buffer persists across flushes:
//   - steady frames create nothing and upload nothing
//   - a changed transform uploads exactly once, same buffer
//   - outgrowing the buffer creates exactly one new one
//   - multiply() matches raymath's MatrixMultiply, and each frame's pixels
//     match one draw_model per copy
//
// Opens a hidden window: models and render targets need a GL context.

#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace raylib {
#include <raylib.h>
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
  // multiply() against raymath.
  {
    std::mt19937 rng(11);
    std::uniform_real_distribution<float> unit(-2.f, 2.f);
    float worst = 0.f;
    for (int i = 0; i < 200; i++) {
      raylib::Matrix a{}, b{};
      float *pa = &a.m0, *pb = &b.m0;
      for (int k = 0; k < 16; k++) pa[k] = unit(rng);
      for (int k = 0; k < 16; k++) pb[k] = unit(rng);
      worst = std::fmax(worst, max_abs_diff(multiply(a, b), raylib::MatrixMultiply(a, b)));
    }
    check(worst < 1e-4f, "multiply matches raymath MatrixMultiply");
  }

  raylib::SetTraceLogLevel(raylib::LOG_ERROR);
  raylib::SetConfigFlags(raylib::FLAG_WINDOW_HIDDEN);
  raylib::InitWindow(64, 64, "instance_batcher_persist_test");
  auto &models = ModelLibrary::get();
  check(models.load(AFTERHOURS_MODELS_DIR "/tree-large.glb", "tree"), "tree loads");
  check(models.load(AFTERHOURS_MODELS_DIR "/kaykit/lantern.gltf", "lantern"),
        "lantern loads");
  if (!models.contains("tree") || !models.contains("lantern")) {
    raylib::CloseWindow();
    printf("instance_batcher_persist_test: %d/%d checks passed\n", checks_passed, checks_run);
    return 1;
  }
  const ModelType &tree = models.get("tree");
  const ModelType &lantern = models.get("lantern");

  struct Copy {
    const ModelType *model;
    raylib::Vector3 pos;
    float yaw;
    float scale;
  };
  std::vector<Copy> scene = {
      {&tree, {-3, 0, 0}, 0.f, 1.f},     {&lantern, {-1, 0, 1}, 30.f, 1.f},
      {&tree, {1, 0, -1}, 90.f, 0.8f},   {&lantern, {3, 0, 0}, 200.f, 1.2f},
      {&tree, {0, 0, 2}, 45.f, 1.1f},
  };

  raylib::Camera3D cam{};
  cam.position = {0, 6, 10};
  cam.target = {0, 1, 0};
  cam.up = {0, 1, 0};
  cam.fovy = 45.f;
  cam.projection = raylib::CAMERA_PERSPECTIVE;
  raylib::RenderTexture2D rt = raylib::LoadRenderTexture(256, 256);
  raylib::Shader shader = load_instancing_shader();
  auto render = [&](bool batched, InstanceBatcher &batcher) {
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
  auto pixels_match = [&](InstanceBatcher &batcher) {
    raylib::Image batched = render(true, batcher);
    raylib::Image direct = render(false, batcher);
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
    bool ok = covered > 2000 && differ <= covered / 200;
    raylib::UnloadImage(batched);
    raylib::UnloadImage(direct);
    return ok;
  };
  auto fill = [&](InstanceBatcher &b) {
    for (const Copy &c : scene)
      b.add(*c.model, c.pos, c.yaw, {c.scale, c.scale, c.scale});
  };

  {
    InstanceBatcher batcher;
    auto frame = [&](const char *what) {
      fill(batcher);
      check(pixels_match(batcher), what);
    };
    // Frame one: one create per group (tree + lantern), no uploads beyond it.
    frame("frame one pixels match");
    check(batcher.buffer_creates() == 2, "first frame creates one buffer per group");
    check(batcher.buffer_uploads() == 0, "buffer first fill rides with its create");
    // Steady frames: nothing created, nothing uploaded.
    frame("steady frame pixels match");
    frame("second steady frame pixels match");
    check(batcher.buffer_creates() == 2, "steady frames create nothing");
    check(batcher.buffer_uploads() == 0, "steady frames upload nothing");
    // Empty flush keeps the buffers.
    batcher.flush(shader);
    check(batcher.buffer_creates() == 2, "empty flush keeps buffers");
    // Same count, one changed transform: exactly one upload.
    scene[0].pos.x += 0.25f;
    frame("moved frame pixels match");
    check(batcher.buffer_creates() == 2, "change reuses the buffer");
    check(batcher.buffer_uploads() == 1, "change uploads exactly once");
    // Growth past capacity: one new create.
    const Copy extra{&tree, {2, 0, 2}, 10.f, 0.9f};
    scene.push_back(extra);
    frame("grown frame pixels match");
    check(batcher.buffer_creates() == 3, "growth creates one new buffer");
    check(batcher.buffer_uploads() == 1, "growth adds no uploads");
  }

  raylib::UnloadRenderTexture(rt);
  raylib::UnloadShader(shader);
  models.unload_all();
  raylib::CloseWindow();

  printf("instance_batcher_persist_test: %d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
