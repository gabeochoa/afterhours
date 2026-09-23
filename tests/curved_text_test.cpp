#include <afterhours/src/plugins/ui.h>

#include <cmath>
#include <cstdio>
#include <string>

using namespace afterhours;
using namespace afterhours::ui;

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

static bool near(float a, float b, float eps = 1e-3f) {
  return std::fabs(a - b) < eps;
}

static float ten_per_grapheme(std::string_view) { return 10.f; }

int main() {
  printf("Running curved text tests...\n\n");

  const ArcText arc{.radius = 100.f, .start_angle_deg = -90.f, .clockwise = true};
  const auto glyphs = layout_text_on_arc("abc", 200.f, 150.f, arc, ten_per_grapheme);
  check(glyphs.size() == 3, "one curved glyph per grapheme");
  check(near(glyphs[0].angle_deg, -90.f + 2.8648f, 1e-2f) &&
            near(glyphs[1].angle_deg, -90.f + 8.5944f, 1e-2f) &&
            near(glyphs[2].angle_deg, -90.f + 14.324f, 1e-2f),
        "glyph centres advance by width / radius, starting at the start angle");
  {
    bool on_circle = true;
    bool tangent = true;
    for (const CurvedGlyph &g : glyphs) {
      const float d = std::hypot(g.center_x - 200.f, g.center_y - 150.f);
      if (!near(d, 100.f, 1e-2f))
        on_circle = false;
      if (!near(g.rotation_deg, g.angle_deg + 90.f, 1e-2f))
        tangent = false;
    }
    check(on_circle, "every glyph centre sits on the arc radius");
    check(tangent, "glyph rotation follows the tangent (upright at the top)");
    check(near(glyphs[0].rotation_deg, 2.8648f, 1e-2f),
          "the first glyph at the top is nearly unrotated");
  }

  const ArcText ccw{.radius = 100.f, .start_angle_deg = -90.f, .clockwise = false};
  const auto flipped = layout_text_on_arc("abc", 200.f, 150.f, ccw, ten_per_grapheme);
  check(flipped.size() == 3 && flipped[2].angle_deg < flipped[0].angle_deg &&
            near(flipped[0].angle_deg, -90.f - 2.8648f, 1e-2f),
        "counter-clockwise layout advances the other way");

  const std::string mixed = "ae\xcc\x81\xf0\x9f\x87\xba\xf0\x9f\x87\xb8"
                            "b";
  const auto units = layout_text_on_arc(mixed, 0.f, 0.f, arc, ten_per_grapheme);
  check(units.size() == 4 && units[1].span.size() == 3 && units[2].span.size() == 8,
        "combining marks and flag pairs stay single graphemes on the arc");
  check(units.front().span.begin == 0 && units.back().span.end == mixed.size(),
        "curved spans cover the whole string");

  check(layout_text_on_arc("", 0.f, 0.f, arc, ten_per_grapheme).empty(),
        "empty text lays out no glyphs");
  check(layout_text_on_arc("abc", 0.f, 0.f, ArcText{.radius = 0.f}, ten_per_grapheme)
            .empty(),
        "a zero radius lays out no glyphs");

  {
    Arena arena(64 * 1024);
    RenderCommandBuffer buffer(arena);
    std::vector<TextUnitInstance> instances = {
        {"a", 0.f, 0.f, 20.f, Color{255, 255, 255, 255}, 30.f},
        {"b", 12.f, 0.f, 20.f, Color{255, 255, 255, 255}, -15.f},
    };
    buffer.add_text_units({0.f, 0.f, 400.f, 24.f}, instances.data(), instances.size(),
                          "default", 3, 42);
    check(buffer.commands().size() == 1 &&
              near(buffer.commands()[0].data.text_units.units[0].rotation, 30.f) &&
              near(buffer.commands()[0].data.text_units.units[1].rotation, -15.f),
          "per-instance rotation survives the batched TextUnits command");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
