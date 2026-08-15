// styling_test.cpp
// Characterization tests for apply_visuals (component_init.h).
//
// Every widget in the library routes through apply_visuals, and it had no
// direct test — the only safety net was indirect coverage from the widget
// tests. These pin its behaviour so the overlay refactor can be shown to change
// nothing.
//
// Cases 1-3 target the three branches that force a reset rather than skipping
// an unset field. Those are the ones the overlay mode gates, so they are the
// ones most likely to break:
//
//   1. color_usage == Default        -> colour reset to transparent
//   2. HasOpacity                    -> written unconditionally
//   3. no scale/translate            -> HasUIModifiers + absolute pos zeroed
//
// If these three still pass after the refactor, existing widgets are unaffected.

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
bool same_color(const Color &a, const Color &b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}
} // namespace

// --------------------------------------------------------------------------
// Reset branch 1: an element that never sets a colour is forced transparent,
// so a reused entity cannot keep a background from a previous frame.
// --------------------------------------------------------------------------
TEST(unstyled_element_is_transparent) {
  ImmTestHarness h;
  auto d = div(h.context(), mk(h.root(), 0),
               ComponentConfig{}.with_debug_name("plain"));
  h.layout_only();

  CHECK(d.ent().has<HasColor>());
  if (d.ent().has<HasColor>())
    CHECK(same_color(d.ent().get<HasColor>().color(), colors::transparent()));
}

// --------------------------------------------------------------------------
// Reset branch 2: HasOpacity is always present, even when untouched.
// --------------------------------------------------------------------------
TEST(opacity_component_always_present) {
  ImmTestHarness h;
  auto d = div(h.context(), mk(h.root(), 0), ComponentConfig{});
  h.layout_only();

  CHECK(d.ent().has<HasOpacity>());
  if (d.ent().has<HasOpacity>())
    CHECK_APPROX(d.ent().get<HasOpacity>().value, 1.0f);
}

// --------------------------------------------------------------------------
// Reset branch 3: with no scale/translate, modifiers and absolute position are
// zeroed rather than left alone.
// --------------------------------------------------------------------------
TEST(modifiers_reset_when_unset) {
  ImmTestHarness h;
  auto d = div(h.context(), mk(h.root(), 0), ComponentConfig{});
  h.layout_only();

  if (d.ent().has<HasUIModifiers>()) {
    auto &m = d.ent().get<HasUIModifiers>();
    CHECK_APPROX(m.scale, 1.0f);
    CHECK_APPROX(m.translate_x, 0.f);
    CHECK_APPROX(m.translate_y, 0.f);
  }
  CHECK_APPROX(d.cmp().absolute_pos_x, 0.f);
  CHECK_APPROX(d.cmp().absolute_pos_y, 0.f);
}

// A set scale/translate survives (the non-reset side of branch 3).
TEST(modifiers_applied_when_set) {
  ImmTestHarness h;
  auto d = div(h.context(), mk(h.root(), 0),
               ComponentConfig{}.with_scale(1.5f).with_translate(10.f, 20.f));
  h.layout_only();

  CHECK(d.ent().has<HasUIModifiers>());
  if (d.ent().has<HasUIModifiers>()) {
    auto &m = d.ent().get<HasUIModifiers>();
    CHECK_APPROX(m.scale, 1.5f);
    CHECK_APPROX(m.translate_x, 10.f);
    CHECK_APPROX(m.translate_y, 20.f);
  }
}

// --------------------------------------------------------------------------
// Guarded paths: these already skip when unset, so the refactor should not
// touch them. Pinned anyway since they share the function.
// --------------------------------------------------------------------------
TEST(custom_background_applies) {
  ImmTestHarness h;
  const Color teal{60, 150, 150, 255};
  auto d = div(h.context(), mk(h.root(), 0),
               ComponentConfig{}.with_custom_background(teal));
  h.layout_only();

  CHECK(d.ent().has<HasColor>());
  if (d.ent().has<HasColor>())
    CHECK(same_color(d.ent().get<HasColor>().color(), teal));
}

TEST(theme_usage_background_applies) {
  ImmTestHarness h;
  auto d = div(h.context(), mk(h.root(), 0),
               ComponentConfig{}.with_background(Theme::Usage::Primary));
  h.layout_only();

  CHECK(d.ent().has<HasColor>());
  if (d.ent().has<HasColor>())
    CHECK(same_color(d.ent().get<HasColor>().color(),
                     h.context().theme.from_usage(Theme::Usage::Primary,
                                                  false)));
}

TEST(hover_color_applies) {
  ImmTestHarness h;
  const Color base{40, 40, 40, 255};
  const Color hov{90, 90, 90, 255};
  auto d = div(h.context(), mk(h.root(), 0),
               ComponentConfig{}.with_custom_background(base).with_custom_hover_bg(
                   hov));
  h.layout_only();

  CHECK(d.ent().has<HasColor>());
  if (d.ent().has<HasColor>()) {
    auto &hc = d.ent().get<HasColor>();
    CHECK(hc.hover_color.has_value());
    if (hc.hover_color.has_value())
      CHECK(same_color(hc.hover_color.value(), hov));
  }
}

TEST(opacity_applies_when_set) {
  ImmTestHarness h;
  auto d = div(h.context(), mk(h.root(), 0),
               ComponentConfig{}.with_opacity(0.35f));
  h.layout_only();

  CHECK(d.ent().has<HasOpacity>());
  if (d.ent().has<HasOpacity>())
    CHECK_APPROX(d.ent().get<HasOpacity>().value, 0.35f);
}

TEST(rounded_corners_apply) {
  ImmTestHarness h;
  auto d = div(h.context(), mk(h.root(), 0),
               ComponentConfig{}
                   .with_rounded_corners(std::bitset<4>(0b1111))
                   .with_roundness(0.25f));
  h.layout_only();

  CHECK(d.ent().has<HasRoundedCorners>());
}

// The point of the px variant: one value means one radius, whatever it lands
// on. The same roundness fraction does not -- it scales with the short side.
TEST(corner_radius_px_is_size_independent) {
  const RectangleType small{0, 0, 200, 40};
  const RectangleType tall{0, 0, 200, 720};

  // 8px on both, expressed as the fraction each backend draws with.
  CHECK_APPROX(resolve_roundness(8.f, 0.5f, small) * 40.f / 2.f, 8.f);
  CHECK_APPROX(resolve_roundness(8.f, 0.5f, tall) * 200.f / 2.f, 8.f);

  // Same roundness, wildly different radius. This is the confusing part.
  CHECK_APPROX(resolve_roundness(std::nullopt, 0.5f, small), 0.5f);
  CHECK_APPROX(resolve_roundness(std::nullopt, 0.5f, tall), 0.5f);

  // A radius past half the short side is already a pill, and clamps there.
  CHECK_APPROX(resolve_roundness(500.f, 0.f, small), 1.0f);
  // Degenerate rect must not divide by zero.
  CHECK_APPROX(resolve_roundness(8.f, 0.5f, RectangleType{0, 0, 0, 0}), 0.f);
}

TEST(corner_radius_px_reaches_the_component) {
  ImmTestHarness h;
  auto d = div(h.context(), mk(h.root(), 0),
               ComponentConfig{}
                   .with_rounded_corners(std::bitset<4>(0b1111))
                   .with_corner_radius(6.f));
  h.layout_only();

  CHECK(d.ent().has<HasRoundedCorners>());
  CHECK(d.ent().get<HasRoundedCorners>().radius_px.has_value());
  CHECK_APPROX(d.ent().get<HasRoundedCorners>().radius_px.value(), 6.f);
}

TEST(cursor_applies_when_set) {
  ImmTestHarness h;
  auto d = div(h.context(), mk(h.root(), 0),
               ComponentConfig{}.with_cursor(CursorType::Pointer));
  h.layout_only();

  CHECK(d.ent().has<HasCursor>());
  if (d.ent().has<HasCursor>())
    CHECK(d.ent().get<HasCursor>().cursor == CursorType::Pointer);
}

// ==========================================================================
// restyle() — overlay a config onto an element that already exists.
// ==========================================================================

// The contract. If any of the three overlay guards is missed, an empty config
// falls through to a reset branch and clobbers something here.
TEST(restyle_empty_config_changes_nothing) {
  ImmTestHarness h;
  const Color teal{60, 150, 150, 255};
  auto d = div(h.context(), mk(h.root(), 0),
               ComponentConfig{}
                   .with_custom_background(teal)
                   .with_opacity(0.4f)
                   .with_scale(1.5f));
  d.restyle(h.context(), ComponentConfig{});
  h.layout_only();

  CHECK(same_color(d.ent().get<HasColor>().color(), teal));
  CHECK_APPROX(d.ent().get<HasOpacity>().value, 0.4f);
  CHECK(d.ent().has<HasUIModifiers>());
  if (d.ent().has<HasUIModifiers>())
    CHECK_APPROX(d.ent().get<HasUIModifiers>().scale, 1.5f);
}

// The immediate-mode invariant: a restyle applied one frame and not the next
// must not survive, because the base config re-applies and resets first.
TEST(restyle_does_not_leak_across_frames) {
  ImmTestHarness h;
  const Color base{20, 20, 20, 255};
  const Color red{200, 40, 40, 255};

  // mk() hashes the call's source location, so both frames have to build from
  // the same line to land on the same entity — hence the lambda.
  const auto build_frame = [&](bool restyled) {
    auto d = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}.with_custom_background(base));
    if (restyled)
      d.restyle(h.context(), ComponentConfig{}.with_custom_background(red));
    return d;
  };

  auto a = build_frame(true);
  h.layout_only();
  CHECK(same_color(a.ent().get<HasColor>().color(), red));

  auto b = build_frame(false);
  h.layout_only();
  CHECK(b.id() == a.id()); // element really was reused
  CHECK(same_color(b.ent().get<HasColor>().color(), base));
}

// A partial overlay touches only what it names.
TEST(restyle_partial_leaves_other_fields) {
  ImmTestHarness h;
  const Color teal{60, 150, 150, 255};
  auto d = div(h.context(), mk(h.root(), 0),
               ComponentConfig{}.with_custom_background(teal));
  d.restyle(h.context(), ComponentConfig{}.with_opacity(0.25f));
  h.layout_only();

  CHECK_APPROX(d.ent().get<HasOpacity>().value, 0.25f);
  CHECK(same_color(d.ent().get<HasColor>().color(), teal)); // untouched
}

// Auto-contrast reads HasLabel::background_hint, so restyling the background
// has to refresh it or the text colour goes stale against the new fill.
TEST(restyle_refreshes_auto_contrast_hint) {
  ImmTestHarness h;
  const Color dark{20, 20, 20, 255};
  const Color light{240, 240, 240, 255};
  auto d = div(h.context(), mk(h.root(), 0),
               ComponentConfig{}
                   .with_label("hello")
                   .with_custom_background(dark)
                   .with_auto_text_color(true));
  d.restyle(h.context(), ComponentConfig{}.with_custom_background(light));
  h.layout_only();

  CHECK(d.ent().has<HasLabel>());
  if (d.ent().has<HasLabel>()) {
    auto &hint = d.ent().get<HasLabel>().background_hint;
    CHECK(hint.has_value());
    if (hint.has_value())
      CHECK(same_color(hint.value(), light));
  }
}

// Proves the overlay path actually reaches apply_visuals. The rest of the
// vocabulary is that function's existing behaviour, pinned by cases 1-4 above.
TEST(restyle_background_custom_and_theme) {
  ImmTestHarness h;
  const Color navy{38, 42, 58, 255};

  auto a = div(h.context(), mk(h.root(), 0), ComponentConfig{});
  a.restyle(h.context(), ComponentConfig{}.with_custom_background(navy));

  auto b = div(h.context(), mk(h.root(), 1), ComponentConfig{});
  b.restyle(h.context(),
            ComponentConfig{}.with_background(Theme::Usage::Primary));
  h.layout_only();

  CHECK(same_color(a.ent().get<HasColor>().color(), navy));
  CHECK(same_color(b.ent().get<HasColor>().color(),
                   h.context().theme.from_usage(Theme::Usage::Primary, false)));
}

int main() { return ui_test::run_registered_tests("styling / apply_visuals"); }
