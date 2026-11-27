
#include <cmath>
#include <cstdio>

#include <afterhours/src/plugins/time_of_day.h>

using namespace afterhours::time_of_day;

static int checks_run = 0;
static int checks_passed = 0;
static void check(bool cond, const char *expr, int line) {
  checks_run++;
  if (cond)
    checks_passed++;
  else
    fprintf(stderr, "  FAIL: %s  (line %d)\n", expr, line);
}
#define CHECK(expr) check((expr), #expr, __LINE__)

static bool near(float a, float b, float eps = 0.02f) {
  return std::fabs(a - b) <= eps;
}

int main() {
  {
    TimeOfDay tod;
    tod.hour = 12.f;
    Lighting l = tod.evaluate();
    CHECK(near(l.sun_elevation_deg, 65.f, 1.f));
    CHECK(l.sun_direction.y > 0.8f);
    CHECK(near(l.sun_intensity, 1.f, 0.05f));
    float len = std::sqrt(l.sun_direction.x * l.sun_direction.x +
                          l.sun_direction.y * l.sun_direction.y +
                          l.sun_direction.z * l.sun_direction.z);
    CHECK(near(len, 1.f, 1e-4f));
  }
  {
    TimeOfDay tod;
    tod.hour = 6.f;
    Lighting l = tod.evaluate();
    CHECK(near(l.sun_elevation_deg, 0.f, 1.f));
    CHECK(l.sun_direction.x > 0.9f);
    tod.hour = 18.f;
    l = tod.evaluate();
    CHECK(near(l.sun_elevation_deg, 0.f, 1.f));
    CHECK(l.sun_direction.x < -0.9f);
  }
  {
    TimeOfDay tod;
    tod.hour = 0.f;
    Lighting l = tod.evaluate();
    CHECK(l.sun_elevation_deg < -10.f);
    CHECK(near(l.sun_intensity, 0.f, 1e-4f));
    CHECK(l.ambient_intensity < 0.2f);
  }
  {
    TimeOfDay tod;
    tod.hour = 9.25f;
    Lighting l = tod.evaluate();
    CHECK(l.sun_intensity > 0.35f && l.sun_intensity < 1.f);
    tod.hour = 12.f;
    l = tod.evaluate();
    CHECK(near(l.sun_intensity, 1.f, 1e-4f));
    CHECK(near(l.fog_density, 0.008f, 1e-4f));
  }
  {
    TimeOfDay tod;
    tod.hour = 22.f;
    Lighting l = tod.evaluate();
    CHECK(l.sun_intensity < 0.05f);
    CHECK(l.ambient_intensity > 0.12f && l.ambient_intensity < 0.2f);
  }
  {
    TimeOfDay tod;
    tod.keys = {{0.f, {255, 0, 0, 255}, 2.f, {0, 0, 0, 255}, 0.f, {0, 0, 0, 255}, 0.f},
                {12.f, {0, 0, 255, 255}, 4.f, {255, 255, 255, 255}, 1.f, {255, 255, 255, 255}, 1.f}};
    tod.hour = 6.f;
    Lighting l = tod.evaluate();
    CHECK(near(l.sun_intensity, 3.f, 1e-4f));
    // colors::lerp works on Color bytes and truncates toward zero, so the
    // halfway blend of 255 and 0 is 128 from the red side and 127 from blue.
    CHECK(l.sun_color.r == 128 && l.sun_color.b == 127);
    CHECK(near(l.ambient_intensity, 0.5f, 1e-4f));
    CHECK(near(l.fog_density, 0.5f, 1e-4f));
  }
  {
    // Default cycle ships byte Colors: noon sun/fog match the keyframe.
    TimeOfDay tod;
    tod.hour = 12.f;
    Lighting l = tod.evaluate();
    CHECK(l.sun_color.r == 255 && l.sun_color.g == 245 &&
          l.sun_color.b == 224);
    CHECK(l.fog_color.r == 191 && l.fog_color.g == 209 &&
          l.fog_color.b == 242);
  }
  {
    TimeOfDay tod;
    tod.hour = 23.5f;
    tod.advance(0.5f, 2.f);
    CHECK(near(tod.hour, 0.5f, 1e-4f));
    tod.advance(1.f, -1.f);
    CHECK(near(tod.hour, 23.5f, 1e-4f));
    tod.advance(24.f, 1.f);
    CHECK(near(tod.hour, 23.5f, 1e-4f));
  }
  {
    TimeOfDay tod;
    tod.keys.clear();
    tod.hour = 12.f;
    Lighting l = tod.evaluate();
    CHECK(near(l.sun_intensity, 0.f, 1e-4f));
    CHECK(l.sun_direction.y > 0.f);
  }

  printf("  %d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
