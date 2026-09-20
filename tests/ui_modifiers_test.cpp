#include <afterhours/src/plugins/ui/components.h>

#include <cmath>
#include <cstdio>
#include <string>


using afterhours::ui::HasUIModifiers;

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

static bool near(float a, float b) { return std::fabs(a - b) < 1e-3f; }

int main() {
  printf("Running ui modifiers tests...\n\n");
  const RectangleType r{100.f, 200.f, 400.f, 80.f};

  {
    HasUIModifiers m;
    m.scale = 1.5f;
    const auto out = m.apply_modifier(r);
    check(near(out.x, 0.f) && near(out.y, 180.f) && near(out.width, 600.f) &&
              near(out.height, 120.f),
          "default origin scales about the centre");
  }
  {
    HasUIModifiers m;
    m.scale = 1.5f;
    m.origin_x = 0.f;
    m.origin_y = 0.f;
    const auto out = m.apply_modifier(r);
    check(near(out.x, 100.f) && near(out.y, 200.f) && near(out.width, 600.f),
          "origin 0,0 keeps the top-left corner pinned");
  }
  {
    HasUIModifiers m;
    m.scale = 0.5f;
    m.origin_x = 1.f;
    m.origin_y = 1.f;
    const auto out = m.apply_modifier(r);
    check(near(out.x + out.width, 500.f) && near(out.y + out.height, 280.f),
          "origin 1,1 keeps the bottom-right corner pinned");
  }
  {
    HasUIModifiers m;
    m.scale = 2.f;
    m.origin_x = 0.f;
    m.translate_x = 10.f;
    const auto out = m.apply_modifier(r);
    check(near(out.x, 110.f) && near(out.width, 800.f),
          "translate applies after the pinned scale");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
