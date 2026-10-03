#include <afterhours/src/plugins/color.h>

#include <cmath>
#include <cstdio>
#include <string>

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

static bool near_color(const afterhours::Color &a,
                       const afterhours::Color &b, int tol = 1) {
  return std::abs(a.r - b.r) <= tol && std::abs(a.g - b.g) <= tol &&
         std::abs(a.b - b.b) <= tol && a.a == b.a;
}

int main() {
  printf("Running color_value tests...\n\n");

  using namespace afterhours::colors;
  using afterhours::Color;

  const Color accent{60, 101, 156, 255};

  check(to_hex_string(accent) == "3C659C", "hex formats without alpha");
  check(to_hex_string(Color{60, 101, 156, 128}, true) == "3C659C80",
        "hex formats with alpha on request");
  check(to_hex_string(Color{0, 0, 0, 255}) == "000000", "black formats");

  const auto six = parse_hex("3C659C");
  check(six.has_value() && near_color(*six, accent, 0) && six->a == 255,
        "six digits parse opaque");
  const auto hashed = parse_hex("#3c659c");
  check(hashed.has_value() && near_color(*hashed, accent, 0),
        "hash and lowercase parse");
  const auto eight = parse_hex("3C659C80");
  check(eight.has_value() && eight->a == 128, "eight digits parse alpha");
  const auto three = parse_hex("#36C");
  check(three.has_value() && three->r == 0x33 && three->g == 0x66 &&
            three->b == 0xCC,
        "three digits double up");

  for (const char *bad : {"", "12", "12345", "1234567", "GGGGGG", "3C 659C",
                          "#", "##3C659C"}) {
    check(!parse_hex(bad).has_value(),
          std::string("rejects: ") + (bad[0] ? bad : "(empty)"));
  }

  const HSV red = to_hsv(Color{255, 0, 0, 255});
  check(std::abs(red.h) < 0.5f && std::abs(red.s - 1.f) < 0.01f &&
            std::abs(red.v - 1.f) < 0.01f,
        "red is hue 0, full saturation and value");
  const HSV gray_hsv = to_hsv(Color{128, 128, 128, 255});
  check(gray_hsv.s < 0.01f, "gray has no saturation");
  check(to_hsv(Color{0, 0, 0, 255}).v < 0.01f, "black has no value");

  for (const Color c :
       {accent, Color{255, 213, 79, 255}, Color{46, 125, 107, 200},
        Color{176, 58, 72, 255}, Color{10, 20, 30, 255}}) {
    const HSV hsv = to_hsv(c);
    check(near_color(from_hsv(hsv, c.a), c), "hsv round-trips");
  }
  check(near_color(from_hsv(HSV{360.f, 1.f, 1.f}, 255), Color{255, 0, 0, 255}, 0),
        "hue 360 wraps to red");
  check(near_color(from_hsv(HSV{-120.f, 1.f, 1.f}, 255), Color{0, 0, 255, 255}, 0),
        "negative hue wraps");

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
