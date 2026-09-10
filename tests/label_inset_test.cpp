// label_inset_test.cpp
// The 5px label inset used to be an unnamed literal, so apps hardcoded their
// own copy and the two drifted. Contract: the inset is readable, and padding
// that does nothing says so.

#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/rendering.h>

#include <cstdio>
#include <string>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

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


// Measured, not taken on trust: wm has 541 elements setting it, so the answer
// decides whether warning about them is right.
static void padding_probe() {
  float sizes[2] = {0.f, 0.f};
  int i = 0;
  for (float pad : {12.f, 40.f}) {
    ui_test::ImmTestHarness h;
    button(h.context(), mk(h.root(), 0),
                ComponentConfig{}
                    .with_label("hello")
                    .with_padding(Padding{.top = pixels(pad),
                                          .left = pixels(pad),
                                          .bottom = pixels(pad),
                                          .right = pixels(pad)})
                    .with_debug_name("pad_probe"));
    h.layout_only();
    UIComponent *c = h.find("pad_probe");
    sizes[i++] = c ? c->rect().width : -1.f;
  }
  printf("  label-only width at pad 12 = %.1f, at pad 40 = %.1f\n", sizes[0],
         sizes[1]);
  // Identical: accepted, stored, ignored. If padding ever starts working the
  // warning has to go with it.
  check(sizes[0] == sizes[1],
        "padding on a label-only element does nothing, hence the warning");
}

int main() {
  printf("=== label inset ===\n\n");

  padding_probe();

  check(kTextInset == 5.f, "the inset is exposed, not buried in rendering.h");

  {
    const RectangleType box{0.f, 0.f, 200.f, 40.f};
    const Vector2Type inset = text_inset_for(box);
    printf("  200x40 -> inset %.1f,%.1f\n", inset.x, inset.y);
    check(inset.x == kTextInset && inset.y == kTextInset,
          "a roomy box gets the whole inset");
  }

  // Too small to spend 5px, so a caller has to ask rather than assume.
  {
    const RectangleType tight{0.f, 0.f, 8.f, 6.f};
    const Vector2Type inset = text_inset_for(tight);
    printf("  8x6 -> inset %.1f,%.1f\n", inset.x, inset.y);
    check(inset.x < kTextInset && inset.y < kTextInset,
          "a tight box gets less, so the constant alone is not the answer");
    check(inset.x <= tight.width * 0.4f + 0.01f,
          "and never more than 40% of the box");
  }

  {
    const Vector2Type inset = text_inset_for(RectangleType{0.f, 0.f, 0.f, 0.f});
    check(inset.x == 0.f && inset.y == 0.f, "a zero box insets by nothing");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
