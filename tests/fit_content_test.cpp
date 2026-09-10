// fit_content_test.cpp
// hanabi #136: nothing sized a box to its own text. The pieces were all there
// -- Dim::Text, max_width -- but four have to agree, and three of four caps
// the width and silently does not wrap. So: bundled, not a new Dim.

#include "ui_test_harness.h"

#include <cstdio>
#include <string>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

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

static constexpr float kCap = 300.f;

static Vector2Type bubble(const std::string &text, const char *name) {
  ImmTestHarness h;
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_label(text)
          .with_fit_content(pixels(kCap), pixels(16.f))
          .with_debug_name(name));
  h.layout_only();
  UIComponent *c = h.find(name);
  if (!c) return Vector2Type{-1.f, -1.f};
  return Vector2Type{c->rect().width, c->rect().height};
}

// Spelled out. If these disagree the helper has drifted.
static Vector2Type bubble_by_hand(const std::string &text, const char *name) {
  ImmTestHarness h;
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_label(text)
          .with_font_size(pixels(16.f))
          .with_size(ComponentSize{Size{Dim::Text}, Size{Dim::Text}})
          .with_max_width(pixels(kCap))
          .with_text_overflow(TextOverflow::Wrap)
          .with_debug_name(name));
  h.layout_only();
  UIComponent *c = h.find(name);
  if (!c) return Vector2Type{-1.f, -1.f};
  return Vector2Type{c->rect().width, c->rect().height};
}

int main() {
  printf("=== fit_content ===\n\n");

  const Vector2Type shorty = bubble("hi", "short");
  const Vector2Type longer = bubble("a somewhat longer message", "longer");
  const Vector2Type huge = bubble(
      "a message far longer than the cap allows, which has to wrap onto "
      "several lines instead of running off the side of the window",
      "huge");

  printf("  'hi'          %.0f x %.0f\n", shorty.x, shorty.y);
  printf("  longer        %.0f x %.0f\n", longer.x, longer.y);
  printf("  over the cap  %.0f x %.0f\n", huge.x, huge.y);

  check(shorty.x > 0.f && shorty.x < kCap, "a short string hugs its own text");
  check(longer.x > shorty.x, "a longer one is wider, so it is measuring");
  check(huge.x <= kCap + 0.5f, "and one over the cap stops at the cap");
  check(huge.y > shorty.y, "wrapping onto more lines makes it taller");

  const Vector2Type by_hand = bubble_by_hand(
      "a message far longer than the cap allows, which has to wrap onto "
      "several lines instead of running off the side of the window",
      "by_hand");
  printf("  spelled out   %.0f x %.0f\n", by_hand.x, by_hand.y);
  check(by_hand.x == huge.x && by_hand.y == huge.y,
        "with_fit_content matches the composition it stands for");

  // Missing the font size fails quietly: the width still caps.
  {
    ImmTestHarness h;
    div(h.context(), mk(h.root(), 0),
        ComponentConfig{}
            .with_label("a message far longer than the cap allows, which "
                        "would wrap if it could measure itself")
            .with_size(ComponentSize{Size{Dim::Text}, Size{Dim::Text}})
            .with_max_width(pixels(kCap))
            .with_text_overflow(TextOverflow::Wrap)
            .with_debug_name("no_font_size"));
    h.layout_only();
    UIComponent *c = h.find("no_font_size");
    check(c != nullptr, "the no-font-size box exists");
    if (c) {
      printf("  no font size  %.0f x %.0f\n", c->rect().width,
             c->rect().height);
      check(c->rect().height < huge.y,
            "three of the four settings does not wrap, which is the trap");
    }
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
