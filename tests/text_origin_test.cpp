
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

static float drawn_right_x(bool batched, float dx) {
  ImmTestHarness h;
  div(h.context(), mk(h.root(), 0),
      positioned(10.f, 20.f, 200.f, 40.f, TextAlignment::Right)
          .with_text_offset(dx));
  if (batched)
    h.render_batched();
  else
    h.render();
  auto texts = h.drawn("text");
  return texts.empty() ? -9999.f : texts.front().rect.x;
}

TEST(right_text_offset_shifts_origin_left) {
  ImmTestHarness h;
  auto base = div(h.context(), mk(h.root(), 0),
                  positioned(10.f, 20.f, 200.f, 40.f, TextAlignment::Right));
  auto shifted = div(h.context(), mk(h.root(), 0),
                     positioned(10.f, 80.f, 200.f, 40.f, TextAlignment::Right)
                         .with_text_offset(-16.f));
  h.layout_only();
  auto *fonts = h.render_font();
  auto bo = resolved_text_origin(base.ent(), *fonts, h.context().theme,
                                 (float)h.context().screen_height);
  auto so = resolved_text_origin(shifted.ent(), *fonts, h.context().theme,
                                 (float)h.context().screen_height);
  CHECK(bo.has_value() && so.has_value());
  if (!bo || !so) return;
  CHECK_APPROX(so->origin.x, bo->origin.x - 16.f);
}

TEST(left_text_offset_shifts_origin_right) {
  ImmTestHarness h;
  auto base = div(h.context(), mk(h.root(), 0),
                  positioned(10.f, 20.f, 200.f, 40.f, TextAlignment::Left));
  auto shifted = div(h.context(), mk(h.root(), 0),
                     positioned(10.f, 80.f, 200.f, 40.f, TextAlignment::Left)
                         .with_text_offset(12.f));
  h.layout_only();
  auto *fonts = h.render_font();
  auto bo = resolved_text_origin(base.ent(), *fonts, h.context().theme,
                                 (float)h.context().screen_height);
  auto so = resolved_text_origin(shifted.ent(), *fonts, h.context().theme,
                                 (float)h.context().screen_height);
  CHECK(bo.has_value() && so.has_value());
  if (!bo || !so) return;
  CHECK_APPROX(so->origin.x, bo->origin.x + 12.f);
}

TEST(center_text_offset_shifts_origin_right) {
  ImmTestHarness h;
  auto base = div(h.context(), mk(h.root(), 0),
                  positioned(10.f, 20.f, 200.f, 40.f, TextAlignment::Center));
  auto shifted = div(h.context(), mk(h.root(), 0),
                     positioned(10.f, 80.f, 200.f, 40.f, TextAlignment::Center)
                         .with_text_offset(10.f));
  h.layout_only();
  auto *fonts = h.render_font();
  auto bo = resolved_text_origin(base.ent(), *fonts, h.context().theme,
                                 (float)h.context().screen_height);
  auto so = resolved_text_origin(shifted.ent(), *fonts, h.context().theme,
                                 (float)h.context().screen_height);
  CHECK(bo.has_value() && so.has_value());
  if (!bo || !so) return;
  CHECK_APPROX(so->origin.x, bo->origin.x + 10.f);
}

TEST(text_offset_y_shifts_down_and_survives_overrides) {
  ComponentConfig base;
  ComponentConfig over;
  over.with_text_offset(0.f, 4.f);
  base.apply_overrides_from(over);
  CHECK(base.text_x_offset == 0.f);
  CHECK(base.text_y_offset == 4.f);
  ComponentConfig over_x;
  over_x.with_text_offset(-16.f, 2.f);
  ComponentConfig base_x;
  base_x.apply_overrides_from(over_x);
  CHECK(base_x.text_x_offset == -16.f);
  CHECK(base_x.text_y_offset == 2.f);

  ImmTestHarness h;
  auto box = div(h.context(), mk(h.root(), 0),
                 positioned(10.f, 20.f, 200.f, 40.f, TextAlignment::Left)
                     .with_text_offset(0.f, 4.f));
  h.layout_only();
  auto *fonts = h.render_font();
  auto origin = resolved_text_origin(box.ent(), *fonts, h.context().theme,
                                     (float)h.context().screen_height);
  CHECK(origin.has_value());
  if (!origin) return;
  CHECK_APPROX(origin->origin.y,
               20.f + 4.f + 5.f + (40.f - 10.f - 16.f) * 0.5f);
}

TEST(right_text_offset_renders_shifted_both_renderers) {
  const float imm_base = drawn_right_x(false, 0.f);
  const float imm_shifted = drawn_right_x(false, -16.f);
  const float bat_base = drawn_right_x(true, 0.f);
  const float bat_shifted = drawn_right_x(true, -16.f);
  CHECK(imm_base > 0.f && bat_base > 0.f);
  CHECK_APPROX(imm_shifted, imm_base - 16.f);
  CHECK_APPROX(bat_shifted, bat_base - 16.f);
  CHECK_APPROX(bat_base, imm_base);
}

int main() { return ui_test::run_registered_tests("text origin"); }
