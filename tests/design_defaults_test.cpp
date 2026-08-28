// design_defaults_test.cpp
// The library-level findings from docs/DESIGN_REVIEW.md, written as executable
// claims instead of prose. Each TEST asserts the behaviour the review says the
// library SHOULD have, so the ones that are still broken fail here and go green
// when the corresponding fix lands.
//
// These are deliberately about defaults and scales, not about any one screen.
// A screenshot can only show that a screen looks wrong; these say why, at the
// one place every screen inherits it from.
//
// The harness measure stub is width = chars * font_size * 0.5, height =
// font_size, so position_text_ex's returned rect.height IS the font size it
// resolved to. That is what makes the auto-fit findings observable without a
// font or a GPU.

#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/styling_defaults.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
// Resolved font size for `text` auto-fitted into `box`. Auto-fit is the path
// taken whenever font_size is not explicitly set, which is 82 of 101 screens.
float autofit_size(ImmTestHarness &h, const std::string &text, float w,
                   float h_px) {
  return position_text_ex(*h.render_font(), text,
                          RectangleType{0.f, 0.f, w, h_px}, TextAlignment::Left,
                          Vector2Type{0.f, 0.f})
      .rect.height;
}
} // namespace

// ---------------------------------------------------------------------------
// L1. Text auto-fits its box, so font size is a function of string length
// ---------------------------------------------------------------------------

// The finding, reduced to two labels. Same box, same role, different length:
// a type scale says these render at one size. This is what made empire_tycoon
// show three sizes in one card and powerwash_settings alternate between
// shouting and whispering down a single column.
//
// Goes through div() rather than position_text_ex, because the fix is the
// ComponentConfig default; the raw call auto-fits whatever the default is.
TEST(siblings_of_different_label_length_get_the_same_size) {
  ImmTestHarness h;
  auto row = [&](int i, const char *label) {
    return div(h.context(), mk(h.root(), i),
               ComponentConfig{}
                   .with_size(ComponentSize{pixels(200), pixels(40)})
                   .with_label(label)
                   .with_debug_name(label));
  };
  row(0, "Vsync");
  row(1, "Reduced Collision Damage");
  h.render();

  const auto calls = h.drawn("text");
  CHECK(calls.size() == 2);
  if (calls.size() == 2)
    CHECK_APPROX(calls[0].rect.height, calls[1].rect.height);
}

// And auto-fit stays available for the size-constrained cases it suits.
TEST(with_autofit_still_sizes_text_to_its_box) {
  ImmTestHarness h;
  const float shortish = autofit_size(h, "Vsync", 200.f, 40.f);
  const float longish = autofit_size(h, "Reduced Collision Damage", 200.f, 40.f);
  CHECK(shortish > longish);
}

// Auto-fit's floor is MIN_FONT_SIZE = 10.0f, but the library separately
// declares MIN_ACCESSIBLE_SIZE_720P = 16.0f as its own accessibility minimum.
// Nothing enforces the second, so the first silently wins and text lands at
// 10px. Two constants, one of which is a lie.
TEST(autofit_floor_respects_the_declared_accessibility_minimum) {
  CHECK(MIN_FONT_SIZE >= TypographyScale::MIN_ACCESSIBLE_SIZE_720P);
}

// The floor as actually reachable: a long label in a small box bottoms out.
// Whatever the clamp is, it must not produce text below the accessible size.
TEST(a_label_too_long_for_its_box_clamps_no_lower_than_accessible) {
  ImmTestHarness h;
  const float size = autofit_size(h, "Reduced Collision Damage", 60.f, 20.f);
  CHECK(size >= TypographyScale::MIN_ACCESSIBLE_SIZE_720P);
}

// ---------------------------------------------------------------------------
// L2. Two spacing scales exist and they disagree
// ---------------------------------------------------------------------------

// DefaultSpacing is an 8pt grid; the Spacing enum is screen-percentage. 23
// screens use both, so elements land 1.6px apart forever. Resolved at the same
// screen height they must agree, or neither is a scale.
TEST(the_two_spacing_scales_agree) {
  const float screen_h = 720.f;
  struct Pair {
    const char *name;
    Spacing enum_value;
    Size default_value;
  };
  const Pair pairs[] = {
      {"xs/tiny", Spacing::xs, DefaultSpacing::tiny()},
      {"sm/small", Spacing::sm, DefaultSpacing::small()},
      {"md/medium", Spacing::md, DefaultSpacing::medium()},
      {"lg/large", Spacing::lg, DefaultSpacing::large()},
      {"xl/xlarge", Spacing::xl, DefaultSpacing::xlarge()},
  };
  for (const auto &p : pairs) {
    const float from_enum =
        resolve_to_pixels(spacing_to_size(p.enum_value), screen_h);
    const float from_defaults = resolve_to_pixels(p.default_value, screen_h);
    // Sub-pixel, not CHECK_APPROX: its 1.5px tolerance is wider than the
    // 1.6px misalignment this finding is about, so the xs pair (7.2 vs 8)
    // would pass while being exactly the bug.
    const bool agrees = std::fabs(from_enum - from_defaults) < 0.01f;
    if (!agrees)
      fmt::print("  {} disagrees: Spacing={} DefaultSpacing={}\n", p.name,
                 from_enum, from_defaults);
    CHECK(agrees);
  }
}

// screen_pct lands on fractional pixels at 720p, which is where the soft
// off-by-one edges come from. Spacing must resolve to whole pixels.
TEST(spacing_resolves_to_whole_pixels) {
  const float screen_h = 720.f;
  for (Spacing s : {Spacing::xs, Spacing::sm, Spacing::md, Spacing::lg,
                    Spacing::xl}) {
    const float px = resolve_to_pixels(spacing_to_size(s), screen_h);
    const bool whole = std::fabs(px - std::round(px)) < 0.01f;
    if (!whole)
      fmt::print("  Spacing::{} resolves to {} px\n", static_cast<int>(s), px);
    CHECK(whole);
  }
}

// ---------------------------------------------------------------------------
// L3. The semantic palette has `error` but no `success` or `warning`
// ---------------------------------------------------------------------------

// Without these, ToastShowcase maps warning to accent and success to
// secondary, which renders a purple "Success" beside a crimson "Warning" on
// the one screen whose entire subject is semantic colour.
TEST(the_theme_has_success_and_warning) {
  Theme t;
  CHECK(t.is_valid(Theme::Usage::Success));
  CHECK(t.is_valid(Theme::Usage::Warning));
}

// A status colour that equals another status colour carries no status. Green,
// amber and red must be three distinguishable things.
TEST(the_status_colours_are_distinct) {
  Theme t;
  auto same = [](const Color &a, const Color &b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
  };
  CHECK(!same(t.color_ref(Theme::Usage::Success),
              t.color_ref(Theme::Usage::Error)));
  CHECK(!same(t.color_ref(Theme::Usage::Warning),
              t.color_ref(Theme::Usage::Error)));
  CHECK(!same(t.color_ref(Theme::Usage::Success),
              t.color_ref(Theme::Usage::Warning)));
}

int main() { return ui_test::run_registered_tests("design defaults"); }
