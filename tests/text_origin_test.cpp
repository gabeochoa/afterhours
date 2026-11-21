// text_origin_test.cpp
// resolved_text_origin() returns the point handed to DrawTextEx, so a caret
// overlay can ask instead of re-deriving the inset. Each check below proves
// the query matches what the immediate renderer actually drew.

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;
using ui_test::TestInputAction;

static ComponentConfig positioned(float x, float y, float w, float height,
                                 TextAlignment alignment, float inset = 5.f) {
  return ComponentConfig{}
      .with_size({pixels(w), pixels(height)})
      .with_absolute_position(x, y)
      .with_label("hello")
      .with_font(UIComponent::DEFAULT_FONT, pixels(16))
      .with_alignment(alignment)
      .with_text_inset(inset)
      .with_corner_radius(0.f)
      .with_skip_grid_snap(true)
      .with_transparent_bg();
}

TEST(origin_matches_drawn_glyph_start) {
  ImmTestHarness h;
  auto box = div(h.context(), mk(h.root(), 0),
                 positioned(10.f, 20.f, 200.f, 40.f, TextAlignment::Left));
  h.layout_only();
  auto *fonts = h.render_font();
  auto origin = resolved_text_origin(box.ent(), *fonts, h.context().theme,
                                     (float)h.context().screen_height);
  CHECK(origin.has_value());
  if (!origin) return;
  CHECK_APPROX(origin->laid_out_rect.x, 10.f);
  CHECK_APPROX(origin->laid_out_rect.y, 20.f);
  CHECK_APPROX(origin->inset.x, 5.f);
  CHECK_APPROX(origin->font_size, 16.f);
  h.render();
  auto texts = h.drawn("text");
  CHECK(!texts.empty());
  if (texts.empty()) return;
  CHECK_APPROX(texts.front().rect.x, origin->origin.x);
  CHECK_APPROX(texts.front().rect.y, origin->origin.y);
  CHECK_APPROX(texts.front().rect.height, origin->font_size);
  // Left-aligned: origin sits one inset from the box edge.
  CHECK_APPROX(origin->origin.x, 10.f + 5.f);
}

TEST(origin_tracks_alignment) {
  ImmTestHarness h;
  auto left = div(h.context(), mk(h.root(), 0),
                  positioned(10.f, 20.f, 200.f, 40.f, TextAlignment::Left));
  auto right = div(h.context(), mk(h.root(), 0),
                   positioned(10.f, 80.f, 200.f, 40.f, TextAlignment::Right));
  auto centre = div(
      h.context(), mk(h.root(), 0),
      positioned(10.f, 140.f, 200.f, 40.f, TextAlignment::Center));
  h.layout_only();
  auto *fonts = h.render_font();
  auto lo = resolved_text_origin(left.ent(), *fonts, h.context().theme,
                                 (float)h.context().screen_height);
  auto ro = resolved_text_origin(right.ent(), *fonts, h.context().theme,
                                 (float)h.context().screen_height);
  auto co = resolved_text_origin(centre.ent(), *fonts, h.context().theme,
                                 (float)h.context().screen_height);
  CHECK(lo.has_value() && ro.has_value() && co.has_value());
  if (!lo || !ro || !co) return;
  CHECK(co->origin.x > lo->origin.x + 20.f);
  CHECK(ro->origin.x > co->origin.x + 20.f);
  CHECK_APPROX(lo->origin.y, 20.f + 5.f + (40.f - 10.f - 16.f) * 0.5f);
}

TEST(origin_scales_with_ui_scale) {
  ImmTestHarness h;
  h.context().theme.ui_scale = 2.f;
  auto box = div(h.context(), mk(h.root(), 0),
                 positioned(10.f, 20.f, 200.f, 40.f, TextAlignment::Left));
  h.layout_only();
  auto *fonts = h.render_font();
  auto origin = resolved_text_origin(box.ent(), *fonts, h.context().theme,
                                     (float)h.context().screen_height);
  CHECK(origin.has_value());
  if (!origin) return;
  CHECK_APPROX(origin->inset.x, 10.f);
  CHECK_APPROX(origin->origin.x, 10.f + 10.f);
  h.render();
  auto texts = h.drawn("text");
  CHECK(!texts.empty());
  if (!texts.empty())
    CHECK_APPROX(texts.front().rect.x, origin->origin.x);
}

TEST(no_label_returns_nullopt) {
  ImmTestHarness h;
  auto box = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}
                     .with_size({pixels(100), pixels(40)})
                     .with_absolute_position(0.f, 0.f));
  h.layout_only();
  auto *fonts = h.render_font();
  CHECK(!resolved_text_origin(box.ent(), *fonts, h.context().theme,
                              (float)h.context().screen_height)
             .has_value());
}

int main() { return ui_test::run_registered_tests("text origin"); }
