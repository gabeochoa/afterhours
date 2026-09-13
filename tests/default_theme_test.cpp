#include "ui_test_harness.h"
#include <afterhours/src/plugins/toast.h>
#include <afterhours/src/plugins/ui/tooltip.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

namespace {
bool same_color(Color a, Color b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

struct FontDefaults {
  std::string name = UIStylingDefaults::get().default_font_name;
  Size size = UIStylingDefaults::get().default_font_size;
  ~FontDefaults() { UIStylingDefaults::get().set_default_font(name, size); }
};
}

TEST(default_theme_has_readable_text_actions_and_control_edges) {
  const Theme theme;
  CHECK(theme.validate_accessibility());
  for (auto usage : {Theme::Usage::Background, Theme::Usage::Surface,
                     Theme::Usage::Primary, Theme::Usage::Error,
                     Theme::Usage::Success, Theme::Usage::Warning}) {
    CHECK(colors::contrast_ratio(theme.auto_font_for(usage),
                                  theme.from_usage(usage)) >= 4.5f);
  }
  CHECK(colors::contrast_ratio(theme.control_border(), theme.surface) >= 3.f);
  CHECK(colors::contrast_ratio(theme.control_border(theme.secondary), theme.secondary) >= 3.f);
  CHECK(colors::contrast_ratio(theme.control_border(theme.raised_surface()), theme.raised_surface()) >= 3.f);
  CHECK(colors::contrast_ratio(theme.focus, theme.background) >= 3.f);
  CHECK(colors::contrast_ratio(theme.focus, theme.primary) >= 3.f);
}

TEST(surface_tokens_adapt_to_light_themes_and_preserve_overrides) {
  Theme theme;
  theme.surface = Color{248, 250, 253, 255};
  CHECK(colors::luminance(theme.raised_surface()) < colors::luminance(theme.surface));
  CHECK(colors::contrast_ratio(theme.control_border(), theme.surface) >= 3.f);
  const Color custom{123, 75, 29, 255};
  theme.border = custom;
  theme.border_muted = custom;
  theme.surface_raised = custom;
  CHECK(same_color(theme.control_border(), custom));
  CHECK(same_color(theme.subtle_border(), custom));
  CHECK(same_color(theme.raised_surface(), custom));
}

TEST(toasts_use_status_colors_and_fixed_radii) {
  ui_test::ImmTestHarness h;
  h.context().theme.success = Color{23, 101, 69, 255};
  h.context().theme.warning = Color{190, 127, 19, 255};
  h.context().theme.corner_radius = 6.f;
  auto success = toast::schedule(h.context(), toast::Level::Success);
  auto warning = toast::schedule(h.context(), toast::Level::Warning);
  CHECK(same_color(success.ent().get<HasColor>().color(), h.context().theme.success));
  CHECK(same_color(warning.ent().get<HasColor>().color(), h.context().theme.warning));
  CHECK_APPROX(success.ent().get<HasRoundedCorners>().radius_px.value_or(-1.f), 6.f);
  CHECK_APPROX(warning.ent().get<HasRoundedCorners>().radius_px.value_or(-1.f), 6.f);
}

TEST(tooltip_uses_measured_width_matching_font_size_and_requested_placement) {
  ui_test::ImmTestHarness h;
  FontDefaults restore;
  UIStylingDefaults::get().set_default_font("Interface", pixels(20));
  h.context().scaling_mode = ScalingMode::Adaptive;
  auto &fonts_entity = EntityHelper::createPermanentEntity();
  fonts_entity.addComponent<FontManager>().load_font("Interface", get_default_font());
  EntityHelper::registerSingleton<FontManager>(fonts_entity);
  auto &state_entity = EntityHelper::createPermanentEntity();
  auto &state = state_entity.addComponent<TooltipState>();
  EntityHelper::registerSingleton<TooltipState>(state_entity);
  state.showing = 1;
  state.anchor = {200, 200, 40, 40};
  state.text = "Wide WWW and narrow iii";
  state.placement = overlay::Placement::Left;
  set_measure_text_fn([](const char *, float size, float) {
    return Vector2Type{13.f, size};
  });
  clear_draw_calls();
  RenderTooltip<ui_test::TestInputAction> renderer;
  renderer.for_each_with(h.context_entity(), h.context(), 0.f);
  const auto boxes = h.drawn("rectangle_rounded");
  const auto text = h.drawn("text");
  CHECK(boxes.size() == 1);
  CHECK(text.size() == 1);
  if (!boxes.empty()) {
    CHECK_APPROX(boxes.front().rect.width, 29.f);
    CHECK_APPROX(boxes.front().rect.x, 167.f);
  }
  if (!text.empty()) CHECK_APPROX(text.front().rect.height, 20.f);
}

TEST(explicit_no_background_clears_a_previous_status_fill) {
  ui_test::ImmTestHarness h;
  auto emit = [&](Theme::Usage usage) {
    return div(h.context(), mk(h.root(), 0), ComponentConfig{}
        .with_size({pixels(300), pixels(44)}).with_label("Status").with_color_usage(usage).with_auto_text_color(true));
  };
  auto error = emit(Theme::Usage::Error);
  CHECK(error.ent().get<HasColor>().color().a == 255);
  afterhours::ui::imm::detail::apply_restyle(h.context(), error.ent(), ComponentConfig{}.with_color_usage(Theme::Usage::None));
  CHECK(error.ent().get<HasColor>().color().a == 0);
  CHECK(same_color(error.ent().get<HasLabel>().background_hint.value(), h.context().theme.background));
  const auto id = error.ent().id;
  h.begin_frame();
  auto clear = emit(Theme::Usage::None);
  CHECK(clear.ent().id == id);
  CHECK(clear.ent().get<HasColor>().color().a == 0);
}

TEST(toast_centering_uses_actual_width_after_resize) {
  ui_test::ImmTestHarness h;
  EntityHelper::registerSingleton<UIContext<ui_test::TestInputAction>>(h.context_entity());
  auto notice = toast::send_success(h.context(), "Saved");
  EntityHelper::merge_entity_arrays();
  h.layout_only();
  auto *res = EntityHelper::get_singleton_cmp<window_manager::ProvidesCurrentResolution>();
  const auto old_resolution = res->current_resolution;
  const auto old_position = toast::position;
  res->current_resolution = {1920, 1080};
  toast::position = toast::Position::BottomCenter;
  toast::ToastLayoutSystem<ui_test::TestInputAction> layout;
  layout.once(0.f);
  const auto rect = notice.cmp().rect();
  CHECK_APPROX(rect.x + rect.width / 2.f, 960.f);
  CHECK_APPROX(rect.width, 510.f);
  CHECK_APPROX(rect.height, 72.f);
  notice.cmp().set_desired_width(pixels(320)).set_desired_height(pixels(60));
  layout.once(0.f);
  CHECK_APPROX(notice.cmp().rect().width, 320.f);
  CHECK_APPROX(notice.cmp().rect().height, 60.f);
  CHECK(notice.ent().get<HasLabel>().text_inset.has_value());
  toast::position = old_position;
  res->current_resolution = old_resolution;
  EntityHelper::get_default_collection().singletonMap.erase(
      components::get_type_id<UIContext<ui_test::TestInputAction>>());
}

TEST(text_inset_positions_plain_and_styled_text_in_both_renderers) {
  for (bool batched : {false, true}) {
    for (bool styled : {false, true}) {
      ui_test::ImmTestHarness h;
      auto config = ComponentConfig{}.with_size({pixels(300), pixels(48)})
          .with_absolute_position(40, 50).with_label("Saved")
          .with_font_size(pixels(20)).with_text_inset(12, 8);
      if (styled) config.with_styled_label({TextSpan{"Saved", Color{255, 255, 255, 255}}});
      div(h.context(), mk(h.root(), 0), config);
      if (batched) h.render_batched();
      else h.render();
      auto calls = h.drawn("text");
      CHECK(calls.size() == 1);
      if (calls.size() != 1) continue;
      CHECK_APPROX(calls.front().rect.x, 52.f);
      CHECK_APPROX(calls.front().rect.y, 64.f);
    }
  }
}

int main() { return ui_test::run_registered_tests("default theme"); }
