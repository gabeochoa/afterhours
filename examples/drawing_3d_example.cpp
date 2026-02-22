// drawing_3d_example.cpp
// Demonstrates the afterhours 3D drawing helpers with headless rendering
//
// Build:
//   make drawing_3d_example
//
// Run:
//   ./drawing_3d_example

#include <iostream>

// Set up raylib in its own namespace (same pattern as game code)
namespace raylib {
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
} // namespace raylib

#ifndef AFTER_HOURS_USE_RAYLIB
#define AFTER_HOURS_USE_RAYLIB
#endif
#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_ENTITY_QUERY
#define AFTER_HOURS_SYSTEM

#include <afterhours/ah.h>
#include <afterhours/src/developer.h>
#include <afterhours/src/drawing_helpers_3d.h>

using Color = raylib::Color;
using Vector3 = raylib::Vector3;
using Vector2 = raylib::Vector2;
constexpr int CAMERA_PERSPECTIVE = afterhours::camera::PERSPECTIVE;
constexpr int CAMERA_ORTHOGRAPHIC = afterhours::camera::ORTHOGRAPHIC;

static int tests_passed = 0;
static int tests_run = 0;

#define CHECK(cond, msg)                                                       \
  do {                                                                         \
    tests_run++;                                                               \
    if (cond) {                                                                \
      tests_passed++;                                                          \
      std::cout << "  [PASS] " << msg << "\n";                                \
    } else {                                                                   \
      std::cout << "  [FAIL] " << msg << "\n";                                \
    }                                                                          \
  } while (0)

// Verify Camera3D type matches raylib::Camera3D on the raylib backend
void test_camera3d_type() {
  std::cout << "\n--- Camera3D type compatibility ---\n";

  afterhours::Camera3D cam{};
  cam.position = {10.0f, 10.0f, 10.0f};
  cam.target = {0.0f, 0.0f, 0.0f};
  cam.up = {0.0f, 1.0f, 0.0f};
  cam.fovy = 45.0f;
  cam.projection = CAMERA_PERSPECTIVE;

  CHECK(sizeof(afterhours::Camera3D) == sizeof(raylib::Camera3D),
        "Camera3D size matches raylib::Camera3D");

  CHECK(cam.position.x == 10.0f && cam.position.y == 10.0f &&
            cam.position.z == 10.0f,
        "Camera3D position fields accessible");

  CHECK(cam.fovy == 45.0f, "Camera3D fovy field accessible");
  CHECK(cam.projection == CAMERA_PERSPECTIVE,
        "Camera3D projection field accessible");

  // Can assign between types since they're aliased
  raylib::Camera3D rl_cam = cam;
  CHECK(rl_cam.position.x == cam.position.x, "Assignable to raylib::Camera3D");
}

// Verify all 3D drawing functions compile and are callable
void test_3d_draw_calls() {
  std::cout << "\n--- 3D drawing function calls ---\n";

  afterhours::Camera3D cam{};
  cam.position = {10.0f, 10.0f, 10.0f};
  cam.target = {0.0f, 0.0f, 0.0f};
  cam.up = {0.0f, 1.0f, 0.0f};
  cam.fovy = 45.0f;
  cam.projection = CAMERA_PERSPECTIVE;

  raylib::InitWindow(320, 240, "3d_test");
  raylib::BeginDrawing();

  afterhours::begin_3d(cam);

  afterhours::draw_cube(Vector3{0, 0, 0}, 1.0f, 1.0f, 1.0f, WHITE);
  CHECK(true, "draw_cube compiles and runs");

  afterhours::draw_cube_wires(Vector3{2, 0, 0}, 1.0f, 1.0f, 1.0f, RED);
  CHECK(true, "draw_cube_wires compiles and runs");

  afterhours::draw_plane(Vector3{0, 0, 0}, Vector2{10.0f, 10.0f}, GRAY);
  CHECK(true, "draw_plane compiles and runs");

  afterhours::draw_sphere(Vector3{0, 1, 0}, 0.5f, BLUE);
  CHECK(true, "draw_sphere compiles and runs");

  afterhours::draw_sphere_wires(Vector3{0, 1, 0}, 0.6f, 8, 8, GREEN);
  CHECK(true, "draw_sphere_wires compiles and runs");

  afterhours::draw_cylinder(Vector3{3, 0, 0}, 0.2f, 0.5f, 2.0f, 8, YELLOW);
  CHECK(true, "draw_cylinder compiles and runs");

  afterhours::draw_cylinder_wires(Vector3{3, 0, 0}, Vector3{3, 2, 0}, 0.2f,
                                  0.5f, 8, ORANGE);
  CHECK(true, "draw_cylinder_wires compiles and runs");

  afterhours::draw_line_3d(Vector3{-1, 0, -1}, Vector3{1, 0, 1}, RED);
  CHECK(true, "draw_line_3d compiles and runs");

  afterhours::end_3d();
  CHECK(true, "begin_3d / end_3d cycle completes");

  raylib::EndDrawing();
  raylib::CloseWindow();
}

// Verify get_world_to_screen returns plausible values
void test_world_to_screen() {
  std::cout << "\n--- get_world_to_screen ---\n";

  raylib::InitWindow(800, 600, "w2s_test");

  afterhours::Camera3D cam{};
  cam.position = {0.0f, 10.0f, 10.0f};
  cam.target = {0.0f, 0.0f, 0.0f};
  cam.up = {0.0f, 1.0f, 0.0f};
  cam.fovy = 45.0f;
  cam.projection = CAMERA_PERSPECTIVE;

  // The target point should project near screen center
  Vector2 center = afterhours::get_world_to_screen(Vector3{0, 0, 0}, cam);
  CHECK(center.x > 200.0f && center.x < 600.0f,
        "Target projects near horizontal center");
  CHECK(center.y > 100.0f && center.y < 500.0f,
        "Target projects near vertical center");

  // A point behind the camera should return (0,0) or degenerate
  Vector2 behind =
      afterhours::get_world_to_screen(Vector3{0, 10, 20}, cam);
  // Just verify it doesn't crash — value is implementation-defined
  CHECK(true, "Behind-camera projection does not crash");
  (void)behind;

  raylib::CloseWindow();
}

// Verify orthographic projection mode works
void test_orthographic_camera() {
  std::cout << "\n--- Orthographic camera ---\n";

  raylib::InitWindow(320, 240, "ortho_test");
  raylib::BeginDrawing();

  afterhours::Camera3D cam{};
  cam.position = {10.0f, 10.0f, 10.0f};
  cam.target = {0.0f, 0.0f, 0.0f};
  cam.up = {0.0f, 1.0f, 0.0f};
  cam.fovy = 20.0f;
  cam.projection = CAMERA_ORTHOGRAPHIC;

  afterhours::begin_3d(cam);
  afterhours::draw_cube(Vector3{0, 0, 0}, 2.0f, 2.0f, 2.0f, WHITE);
  afterhours::draw_plane(Vector3{0, -1, 0}, Vector2{10, 10}, DARKGRAY);
  afterhours::end_3d();

  CHECK(true, "Orthographic camera renders without error");

  raylib::EndDrawing();
  raylib::CloseWindow();
}

// Verify using-declarations work (game code imports into global scope)
void test_using_declarations() {
  std::cout << "\n--- Using declarations (game-style imports) ---\n";

  using afterhours::begin_3d;
  using afterhours::draw_cube;
  using afterhours::draw_line_3d;
  using afterhours::draw_plane;
  using afterhours::draw_sphere;
  using afterhours::end_3d;
  using afterhours::get_world_to_screen;

  raylib::InitWindow(320, 240, "using_test");
  raylib::BeginDrawing();

  afterhours::Camera3D cam{};
  cam.position = {5.0f, 5.0f, 5.0f};
  cam.target = {0.0f, 0.0f, 0.0f};
  cam.up = {0.0f, 1.0f, 0.0f};
  cam.fovy = 45.0f;
  cam.projection = CAMERA_PERSPECTIVE;

  begin_3d(cam);
  draw_cube(Vector3{0, 0, 0}, 1.0f, 1.0f, 1.0f, RED);
  draw_plane(Vector3{0, -0.5f, 0}, Vector2{5, 5}, GRAY);
  draw_sphere(Vector3{1, 0.5f, 0}, 0.3f, BLUE);
  draw_line_3d(Vector3{-2, 0, 0}, Vector3{2, 0, 0}, GREEN);
  end_3d();

  Vector2 pos = get_world_to_screen(Vector3{0, 0, 0}, cam);
  CHECK(pos.x > 0.0f, "get_world_to_screen via using-declaration works");

  raylib::EndDrawing();
  raylib::CloseWindow();
}

int main() {
  std::cout << "=== Afterhours 3D Drawing Helpers Example ===\n";

  test_camera3d_type();
  test_3d_draw_calls();
  test_world_to_screen();
  test_orthographic_camera();
  test_using_declarations();

  std::cout << "\n=== Results: " << tests_passed << "/" << tests_run
            << " passed ===\n";
  return (tests_passed == tests_run) ? 0 : 1;
}
