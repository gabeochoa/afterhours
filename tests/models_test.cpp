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

  models.unload_all();
  check(models.size() == 0 && !models.contains("tree"),
        "unload_all drops the entries");

  raylib::CloseWindow();

  printf("models_test: %d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
