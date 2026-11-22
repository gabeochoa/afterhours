#include "ui_test_harness.h"

#include <afterhours/src/plugins/toast.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

TEST(toast_inherits_font_defaults_and_retains_them_across_screens) {
  ui_test::ImmTestHarness h;
  auto &defaults = UIStylingDefaults::get();
  const auto saved_name = defaults.default_font_name;
  const auto saved_size = defaults.default_font_size;
  defaults.set_default_font("NotificationFont", h720(18));
  h.context().scaling_mode = ScalingMode::Adaptive;
  auto notice = toast::send_info(h.context(), "Saved");
  defaults.set_default_font("NextScreenFont", pixels(24));
  CHECK(notice.cmp().font_name == "NotificationFont");
  CHECK(notice.cmp().font_size.dim == Dim::ScreenPercent);
  CHECK_APPROX(notice.cmp().font_size.value * 720.f, 18.f);
  CHECK(notice.cmp().font_size_explicitly_set);
  CHECK(notice.cmp().resolved_scaling_mode == ScalingMode::Adaptive);
  defaults.set_default_font(saved_name, saved_size);
}

TEST(toast_has_a_font_when_no_app_default_is_configured) {
  ui_test::ImmTestHarness h;
  auto &defaults = UIStylingDefaults::get();
  const auto saved_name = defaults.default_font_name;
  const auto saved_size = defaults.default_font_size;
  defaults.set_default_font(UIComponent::UNSET_FONT, pixels(16));
  auto notice = toast::send_info(h.context(), "Ready");
  CHECK(notice.cmp().font_name == UIComponent::DEFAULT_FONT);
  CHECK_APPROX(notice.cmp().font_size.value, 16.f);
  defaults.set_default_font(saved_name, saved_size);
}

TEST(toast_text_contrasts_with_its_background_after_theme_changes) {
  ui_test::ImmTestHarness h;
  h.context().theme.font = Color{255, 255, 255, 255};
  h.context().theme.darkfont = Color{30, 30, 30, 255};
  for (Color background : {Color{255, 127, 80, 255},
                           Color{16, 24, 32, 255}}) {
    auto notice = toast::send_custom(h.context(), "Notification", background);
    auto next_theme = h.context().theme;
    next_theme.font = next_theme.darkfont = background;
    const auto foreground = ui::detail::resolve_label_color(
        notice.ent().get<HasLabel>(), next_theme);
    CHECK(colors::contrast_ratio(foreground, background) >= 4.5f);
  }
}

TEST(scheduled_toast_fades_over_its_lifetime) {
  ui_test::ImmTestHarness h;
  EntityHelper::registerSingleton<UIContext<ui_test::TestInputAction>>(
      h.context_entity());
  auto notice =
      toast::schedule(h.context(), toast::Level::Info, 2.0f);
  EntityHelper::merge_entity_arrays();
  CHECK(notice.ent().has<HasOpacity>());
  if (notice.ent().has<HasOpacity>()) {
    CHECK(std::fabs(notice.ent().get<HasOpacity>().value - 1.0f) < 0.001f);
    toast::ToastLayoutSystem<ui_test::TestInputAction> layout;
    toast::ToastUpdateSystem update;
    layout.once(0.f);
    CHECK(std::fabs(notice.ent().get<HasOpacity>().value - 1.0f) < 0.001f);
    update.for_each_with(notice.ent(), notice.ent().get<toast::Toast>(),
                         1.0f);
    layout.once(0.f);
    CHECK(std::fabs(notice.ent().get<HasOpacity>().value - 0.96875f) <
          0.001f);
    update.for_each_with(notice.ent(), notice.ent().get<toast::Toast>(),
                         0.9f);
    layout.once(0.f);
    CHECK(std::fabs(notice.ent().get<HasOpacity>().value -
                    (1.0f - std::pow(2.0f, -0.5f))) < 0.001f);
    update.for_each_with(notice.ent(), notice.ent().get<toast::Toast>(),
                         0.1f);
    layout.once(0.f);
    CHECK(std::fabs(notice.ent().get<HasOpacity>().value) < 0.001f);
  }
  EntityHelper::get_default_collection().singletonMap.erase(
      components::get_type_id<UIContext<ui_test::TestInputAction>>());
}

int main() { return ui_test::run_registered_tests("toast"); }
