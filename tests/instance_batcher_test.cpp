// instance_batcher_test.cpp
// InstanceBatcher and model_transform under raylib:
//   - model_transform matches raymath's scale * rotateY * translate, the
//     composition DrawModelEx uses
//   - copies of two models land in two groups, and flush empties them
//   - a batched frame renders the same pixels as one draw_model per copy
//   - per-instance tint reaches pixels and matches tinted draw_model
//   - instanced / plain / instanced mixing renders every copy (VAO-safe)
//   - flush() with no shader draws through the batcher-owned default shader
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

  // Per-instance tint: same tree twice, red on the left, blue on the right.
  // Batched pixels must match draw_model with the same tints, and the two
  // halves must carry their tints (not just differ from each other).
  {
    struct TintedCopy {
      raylib::Vector3 pos;
      Color tint;
    };
    TintedCopy tinted_scene[] = {{{-2, 0, 0}, {255, 0, 0, 255}},
                                 {{2, 0, 0}, {0, 0, 255, 255}}};
    InstanceBatcher tinted;
    for (const TintedCopy &c : tinted_scene)
      tinted.add(tree, c.pos, 0.f, {1, 1, 1}, 0, c.tint);
    check(tinted.copies() == 2 && tinted.group_count() == 1,
          "tinted copies group by model");
    auto render_tinted = [&](bool batched_tint) {
      raylib::BeginTextureMode(rt);
      raylib::ClearBackground({0, 0, 0, 0});
      raylib::BeginMode3D(cam);
      if (batched_tint) {
        tinted.flush(shader);
      } else {
        for (const TintedCopy &c : tinted_scene)
          draw_model(tree, c.pos, 0.f, {1, 1, 1}, c.tint);
      }
      raylib::EndMode3D();
      raylib::EndTextureMode();
      raylib::Image img = raylib::LoadImageFromTexture(rt.texture);
      raylib::ImageFormat(&img, raylib::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
      return img;
    };
    raylib::Image tint_batched = render_tinted(true);
    raylib::Image tint_direct = render_tinted(false);
    auto *pt = static_cast<raylib::Color *>(tint_batched.data);
    auto *pd = static_cast<raylib::Color *>(tint_direct.data);
    int tint_differ = 0, tint_covered = 0;
    long left_r = 0, left_b = 0, right_r = 0, right_b = 0, left_n = 0, right_n = 0;
    for (int y = 0; y < 256; y++) {
      for (int x = 0; x < 256; x++) {
        int i = y * 256 + x;
        if (pt[i].a > 0 || pd[i].a > 0) tint_covered++;
        int d = std::abs(pt[i].r - pd[i].r) + std::abs(pt[i].g - pd[i].g) +
                std::abs(pt[i].b - pd[i].b) + std::abs(pt[i].a - pd[i].a);
        if (d > 6) tint_differ++;
        if (pt[i].a > 0) {
          if (x < 128) {
            left_r += pt[i].r; left_b += pt[i].b; left_n++;
          } else {
            right_r += pt[i].r; right_b += pt[i].b; right_n++;
          }
        }
      }
    }
    printf("  tint: %d covered, %d differ, left R-B %ld, right R-B %ld\n",
           tint_covered, tint_differ,
           left_n ? (left_r - left_b) / left_n : 0,
           right_n ? (right_r - right_b) / right_n : 0);
    check(tint_covered > 1000, "tinted scene is on screen");
    check(tint_differ <= tint_covered / 200,
          "tinted batched frame matches draw_model with tints");
    check(left_n > 0 && right_n > 0 && left_r > left_b && right_b > right_r,
          "per-instance tint reaches pixels (red left, blue right)");
    raylib::UnloadImage(tint_batched);
    raylib::UnloadImage(tint_direct);
  }

  // Mixed sequence: instanced flush, plain draw_model of the same model,
  // instanced flush again. All four copies must draw and match the
  // all-plain reference, proving the VAO is clean after each flush.
  {
    InstanceBatcher mixed;
    mixed.add(tree, {-3, 0, 0}, 0.f, {0.8f, 0.8f, 0.8f});
    mixed.add(lantern, {3, 0, 0}, 0.f, {1, 1, 1});
    auto render_mixed = [&](bool mixed_mode) {
      raylib::BeginTextureMode(rt);
      raylib::ClearBackground({0, 0, 0, 0});
      raylib::BeginMode3D(cam);
      if (mixed_mode) {
        mixed.flush(shader);
        draw_model(tree, {0, 0, 0}, 45.f, {0.7f, 0.7f, 0.7f});
        mixed.add(tree, {0, 0, 2.5f}, 90.f, {0.6f, 0.6f, 0.6f});
        mixed.flush(shader);
      } else {
        draw_model(tree, {-3, 0, 0}, 0.f, {0.8f, 0.8f, 0.8f});
        draw_model(lantern, {3, 0, 0}, 0.f, {1, 1, 1});
        draw_model(tree, {0, 0, 0}, 45.f, {0.7f, 0.7f, 0.7f});
        draw_model(tree, {0, 0, 2.5f}, 90.f, {0.6f, 0.6f, 0.6f});
      }
      raylib::EndMode3D();
      raylib::EndTextureMode();
      raylib::Image img = raylib::LoadImageFromTexture(rt.texture);
      raylib::ImageFormat(&img, raylib::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
      return img;
    };
    raylib::Image mixed_img = render_mixed(true);
    raylib::Image plain_img = render_mixed(false);
    auto *pm = static_cast<raylib::Color *>(mixed_img.data);
    auto *pp = static_cast<raylib::Color *>(plain_img.data);
    int mixed_covered = 0, mixed_differ = 0;
    for (int i = 0; i < 256 * 256; i++) {
      if (pm[i].a > 0 || pp[i].a > 0) mixed_covered++;
      int d = std::abs(pm[i].r - pp[i].r) + std::abs(pm[i].g - pp[i].g) +
              std::abs(pm[i].b - pp[i].b) + std::abs(pm[i].a - pp[i].a);
      if (d > 6) mixed_differ++;
    }
    printf("  mixed: %d covered, %d differ\n", mixed_covered, mixed_differ);
    check(mixed_covered > 1500, "mixed instanced+plain scene is on screen");
    check(mixed_differ <= mixed_covered / 200,
          "instanced, plain, instanced sequence matches all-plain");
    raylib::UnloadImage(mixed_img);
    raylib::UnloadImage(plain_img);
  }

  // Batcher-owned default shader: flush() with no shader argument draws
  // through the shader the batcher loaded itself.
  {
    InstanceBatcher owned;
    owned.add(tree, {0, 0, 0}, 0.f, {1, 1, 1});
    raylib::BeginTextureMode(rt);
    raylib::ClearBackground({0, 0, 0, 0});
    raylib::BeginMode3D(cam);
    owned.flush();
    raylib::EndMode3D();
    raylib::EndTextureMode();
    check(owned.copies() == 0, "flush() with owned default shader empties groups");
    raylib::Image img = raylib::LoadImageFromTexture(rt.texture);
    raylib::ImageFormat(&img, raylib::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    auto *po = static_cast<raylib::Color *>(img.data);
    int owned_covered = 0;
    for (int i = 0; i < 256 * 256; i++)
      if (po[i].a > 0) owned_covered++;
    printf("  owned shader: %d covered\n", owned_covered);
    check(owned_covered > 500, "owned default shader draws the scene");
    raylib::UnloadImage(img);
  }

  raylib::UnloadRenderTexture(rt);
  raylib::UnloadShader(shader);
  models.unload_all();
  raylib::CloseWindow();

  printf("instance_batcher_test: %d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
