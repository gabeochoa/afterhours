// camera_test.cpp
// Behavior lock for the camera plugin (src/plugins/camera.h).
//
// Build (raylib-backed — camera's real logic is behind AFTER_HOURS_USE_RAYLIB):
//   see tests/Makefile target `camera_test` (RAYLIB_TESTS group).
//
// What this locks (must stay render-neutral across any perf change):
//   1. HasCamera default state + setters produce the exact Camera2D fields the
//      renderer feeds to raylib::BeginMode2D.
//   2. The camera TRANSFORM MATRIX (raylib::GetCameraMatrix2D) — this is the
//      actual render output. Any "speedup" that changes this matrix for a
//      given camera state is a REGRESSION. GetCameraMatrix2D is a pure CPU
//      function (no GL context / window needed), so this runs headless.
//   3. Singleton registration + that BOTH begin/end systems resolve the SAME
//      HasCamera singleton (begin/end pairing operates on one camera).
//
// No window is created: we exercise component state, the pure transform math,
// and singleton wiring — not GL draw calls.
//
// AFTER_HOURS_USE_RAYLIB is supplied by the Makefile (-D flag); do not redefine.

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/camera.h>

#include <cmath>
#include <cstdio>
#include <vector>

using namespace afterhours;

static int tests_run = 0;
static int tests_passed = 0;

struct TestEntry {
  const char *name;
  void (*fn)();
};
static std::vector<TestEntry> &test_registry() {
  static std::vector<TestEntry> r;
  return r;
}
static void register_test(const char *name, void (*fn)()) {
  test_registry().push_back({name, fn});
}
static void check(bool cond, const char *expr, const char *file, int line) {
  tests_run++;
  if (cond)
    tests_passed++;
  else
    fprintf(stderr, "  FAIL: %s  (%s:%d)\n", expr, file, line);
}
#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)
#define CHECK_APPROX(a, b)                                                     \
  check(std::fabs((a) - (b)) < 1e-5f, #a " ~= " #b, __FILE__, __LINE__)

#define TEST(name)                                                             \
  static void test_##name();                                                   \
  struct Register_##name {                                                     \
    Register_##name() { register_test(#name, test_##name); }                   \
  } register_##name##_instance;                                                \
  static void test_##name()

using HasCamera = camera::HasCamera;

// ---------------------------------------------------------------------------
// 1. Default construction matches the documented defaults.
// ---------------------------------------------------------------------------
TEST(default_camera_state) {
  HasCamera hc;
  CHECK_APPROX(hc.camera.offset.x, 0.0f);
  CHECK_APPROX(hc.camera.offset.y, 0.0f);
  CHECK_APPROX(hc.camera.target.x, 0.0f);
  CHECK_APPROX(hc.camera.target.y, 0.0f);
  CHECK_APPROX(hc.camera.rotation, 0.0f);
  CHECK_APPROX(hc.camera.zoom, 0.75f);
}

// ---------------------------------------------------------------------------
// 2. Setters write the exact fields BeginMode2D consumes.
// ---------------------------------------------------------------------------
TEST(setters_update_camera_fields) {
  HasCamera hc;
  hc.set_position(raylib::Vector2{10.0f, 20.0f});
  hc.set_offset(raylib::Vector2{100.0f, 200.0f});
  hc.set_zoom(2.0f);
  hc.set_rotation(45.0f);

  CHECK_APPROX(hc.camera.target.x, 10.0f);
  CHECK_APPROX(hc.camera.target.y, 20.0f);
  CHECK_APPROX(hc.camera.offset.x, 100.0f);
  CHECK_APPROX(hc.camera.offset.y, 200.0f);
  CHECK_APPROX(hc.camera.zoom, 2.0f);
  CHECK_APPROX(hc.camera.rotation, 45.0f);
}

// ---------------------------------------------------------------------------
// 3. RENDER-NEUTRAL LOCK: the transform matrix for a known camera state.
//    GetCameraMatrix2D is exactly what raylib feeds into the GL modelview
//    inside BeginMode2D — locking it locks the render output.
// ---------------------------------------------------------------------------
static bool mat_approx(const raylib::Matrix &a, const raylib::Matrix &b) {
  const float *pa = &a.m0;
  const float *pb = &b.m0;
  for (int i = 0; i < 16; ++i)
    if (std::fabs(pa[i] - pb[i]) > 1e-4f)
      return false;
  return true;
}

TEST(default_camera_transform_matrix) {
  HasCamera hc; // default: zoom 0.75, no offset/target/rotation
  raylib::Matrix m = raylib::GetCameraMatrix2D(hc.camera);
  // Pure scale by zoom on x/y, identity elsewhere.
  raylib::Matrix expected = {};
  expected.m0 = 0.75f;  // scale x
  expected.m5 = 0.75f;  // scale y
  expected.m10 = 1.0f;
  expected.m15 = 1.0f;
  CHECK(mat_approx(m, expected));
}

TEST(offset_target_transform_matrix) {
  HasCamera hc;
  hc.set_position(raylib::Vector2{10.0f, 20.0f}); // target (world focus)
  hc.set_offset(raylib::Vector2{100.0f, 50.0f});  // screen offset
  hc.set_zoom(1.0f);
  hc.set_rotation(0.0f);
  raylib::Matrix m = raylib::GetCameraMatrix2D(hc.camera);
  // With zoom 1, no rotation: translate by (offset - target).
  // raylib builds M = matTranslate(-target) * matRotate * matScale *
  //                   matTranslate(offset); with zoom 1/rot 0 the net
  //                   translation is (offset - target).
  CHECK_APPROX(m.m0, 1.0f);
  CHECK_APPROX(m.m5, 1.0f);
  CHECK_APPROX(m.m12, 100.0f - 10.0f); // tx = 90
  CHECK_APPROX(m.m13, 50.0f - 20.0f);  // ty = 30
}

// ---------------------------------------------------------------------------
// 4. Singleton wiring + begin/end pairing operate on the SAME camera.
//    We do not create a GL window; we verify the plugin's singleton lookups
//    (what BeginCameraMode/EndCameraMode do every frame) resolve identically.
// ---------------------------------------------------------------------------
TEST(singleton_registration_and_pairing) {
  EntityHelper::cleanup();
  // Fresh collection state: register a camera singleton the way the plugin does.
  Entity &e = EntityHelper::createEntity();
  camera::add_singleton_components(e); // adds HasCamera + registerSingleton
  EntityHelper::merge_entity_arrays();

  HasCamera *begin_view = EntityHelper::get_singleton_cmp<HasCamera>();
  HasCamera *end_view = EntityHelper::get_singleton_cmp<HasCamera>();
  CHECK(begin_view != nullptr);
  CHECK(end_view != nullptr);
  // Begin and End must operate on the same camera instance (pairing).
  CHECK(begin_view == end_view);

  // Mutating through the singleton is visible to the next frame's lookup.
  begin_view->set_zoom(3.0f);
  CHECK_APPROX(EntityHelper::get_singleton_cmp<HasCamera>()->camera.zoom, 3.0f);

  EntityHelper::cleanup();
}

int main() {
  printf("=== camera plugin tests ===\n\n");
  for (auto &[name, fn] : test_registry()) {
    printf("  Running: %s\n", name);
    fn();
  }
  printf("\n%d/%d checks passed\n", tests_passed, tests_run);
  return tests_passed == tests_run ? 0 : 1;
}
