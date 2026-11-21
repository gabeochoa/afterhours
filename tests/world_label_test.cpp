// world_label_test.cpp
// project_world_label at a known camera (origin, looking down -Z, 60deg
// vertical fov, 800x600) and the fade curve's endpoints. Pure math: no
// window, either backend.

#define FMT_HEADER_ONLY
#define AFTER_HOURS_USE_NONE
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/world_label.h>
#include <afterhours/src/capture.h>

#include <cmath>
#include <cstdio>
#include <string>

using namespace afterhours;

static int checks = 0;
static int failures = 0;

static void check(bool cond, const std::string &what) {
  ++checks;
  if (!cond) {
    ++failures;
    std::printf("  FAIL: %s\n", what.c_str());
  }
}
static bool near(float a, float b) { return std::fabs(a - b) < 0.01f; }

static Camera3D cam() {
  Camera3D c{};
  c.position = {0, 0, 0};
  c.target = {0, 0, -1};
  c.up = {0, 1, 0};
  c.fovy = 60.f;
  c.projection = camera::PERSPECTIVE;
  return c;
}

int main() {
  // Centre: straight ahead lands mid-screen, rect centred + bottom-anchored.
  {
    auto p = project_world_label(cam(), {0, 0, -10}, 800, 600, {100, 20});
    check(p.visible, "centre point visible");
    check(near(p.screen.x, 400) && near(p.screen.y, 300), "centre is 400,300");
    check(near(p.distance, 10), "centre distance 10");
    check(near(p.rect.x, 350) && near(p.rect.y, 280), "rect centred above anchor");
  }
  // Edge: at 10 ahead, x = z*tan(30)*aspect puts the point on the right edge.
  {
    float edge = 10.f * std::tan(30.f * DEG2RAD) * (800.f / 600.f);
    auto p = project_world_label(cam(), {edge, 0, -10}, 800, 600);
    check(p.visible && near(p.screen.x, 800), "right-edge point at x=800");
    auto q = project_world_label(cam(), {-edge, 0, -10}, 800, 600);
    check(q.visible && near(q.screen.x, 0), "left-edge point at x=0");
    auto off = project_world_label(cam(), {edge * 1.5f, 0, -10}, 800, 600);
    check(!off.visible, "past the edge culled");
  }
  // Behind and on the camera plane: culled, never mirrored to the front.
  {
    check(!project_world_label(cam(), {0, 0, 10}, 800, 600).visible,
          "behind camera culled");
    check(!project_world_label(cam(), {0, 0, 0}, 800, 600).visible,
          "on camera culled");
  }
  // Fade curve: endpoints and midpoint.
  {
    check(near(world_label_alpha(5, 20, 60), 1), "inside near is opaque");
    check(near(world_label_alpha(20, 20, 60), 1), "at near is opaque");
    check(near(world_label_alpha(40, 20, 60), 0.5f), "midpoint is half");
    check(near(world_label_alpha(60, 20, 60), 0), "at far is clear");
    check(near(world_label_alpha(90, 20, 60), 0), "past far stays clear");
  }
  // Draw path: a faded-in label records one text call at the projected spot;
  // a behind-camera label records nothing.
  {
    capture::clear();
    WorldLabel label;
    label.text = "Soup!";
    label.anchor = {0, 0, -10};
    check(draw_world_label(Font{}, cam(), label, 800, 600), "label drawn");
    check(capture::calls().size() == 1 && capture::calls()[0].text == "Soup!",
          "one text call with the label text");
    capture::clear();
    label.anchor = {0, 0, 10};
    check(!draw_world_label(Font{}, cam(), label, 800, 600), "behind not drawn");
    check(capture::calls().empty(), "no call recorded behind camera");
  }
  std::printf("world_label: %d/%d checks passed\n", checks - failures, checks);
  return failures ? 1 : 0;
}
