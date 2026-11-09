// models_test.cpp
// ModelLibrary::load under raylib: a file that does not load must not be
// stored, since raylib returns an empty model instead of failing and
// Library::load would keep it and report success.
//
// Opens a hidden window: LoadModel uploads meshes and needs a GL context.

#include <cstdio>
#include <string>

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

int main() {
  raylib::SetTraceLogLevel(raylib::LOG_ERROR);
  raylib::SetConfigFlags(raylib::FLAG_WINDOW_HIDDEN);
  raylib::InitWindow(64, 64, "models_test");

  auto &models = ModelLibrary::get();

  check(!models.load("does/not/exist.glb", "missing"),
        "missing file reports failure");
  check(!models.contains("missing"), "missing file is not stored");

  check(models.load(AFTERHOURS_MODEL_FIXTURE, "tree"), "fixture loads");
  check(models.contains("tree") && is_model_loaded(models.get("tree")),
        "fixture is stored with meshes");

  check(!models.load(AFTERHOURS_MODEL_FIXTURE, "tree"),
        "duplicate name reports failure");
  check(models.size() == 1, "duplicate name stores nothing new");

  // draw_model straight after draw_model_instanced of the same model. raylib
  // leaves the instance attributes enabled on the mesh's VAO, and until
  // draw_model_instanced turned them off this drew nothing.
  {
    const ModelType &tree = models.get("tree");
    raylib::Shader shader = load_instancing_shader();
    const raylib::Matrix identity = {1, 0, 0, 0, 0, 1, 0, 0,
                                     0, 0, 1, 0, 0, 0, 0, 1};
    raylib::Camera3D cam{};
    cam.position = {0, 2, 5};
    cam.target = {0, 1, 0};
    cam.up = {0, 1, 0};
    cam.fovy = 45.f;
    cam.projection = raylib::CAMERA_PERSPECTIVE;
    raylib::RenderTexture2D rt = raylib::LoadRenderTexture(64, 64);
    raylib::BeginTextureMode(rt);
    raylib::BeginMode3D(cam);
    draw_model_instanced(tree, shader, &identity, 1);
    raylib::EndMode3D();
    raylib::ClearBackground({0, 0, 0, 0});
    raylib::BeginMode3D(cam);
    draw_model(tree, {0, 0, 0}, 0.f, {1, 1, 1});
    raylib::EndMode3D();
    raylib::EndTextureMode();
    raylib::Image img = raylib::LoadImageFromTexture(rt.texture);
    raylib::ImageFormat(&img, raylib::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    auto *px = static_cast<raylib::Color *>(img.data);
    int covered = 0;
    for (int i = 0; i < 64 * 64; i++) covered += px[i].a > 0;
    check(covered > 100, "draw_model works after draw_model_instanced");
    raylib::UnloadImage(img);
    raylib::UnloadRenderTexture(rt);
    raylib::UnloadShader(shader);
  }

  models.unload_all();
  check(models.size() == 0 && !models.contains("tree"),
        "unload_all drops the entries");

  raylib::CloseWindow();

  printf("models_test: %d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
