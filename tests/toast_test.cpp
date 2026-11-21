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
  CHECK(notice.ent().get<HasLabel>().font_name == "NotificationFont");
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
  CHECK(notice.ent().get<HasLabel>().font_name == UIComponent::DEFAULT_FONT);
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

namespace {
int live_toast_count() {
  int n = 0;
  for (Entity &e : EntityQuery().whereHasComponent<toast::Toast>().gen())
    if (!e.cleanup && !e.get<toast::Toast>().is_expired()) n++;
  return n;
}
struct CtxSingleton {
  ui_test::ImmTestHarness &h;
  explicit CtxSingleton(ui_test::ImmTestHarness &harness) : h(harness) {
    EntityHelper::registerSingleton<UIContext<ui_test::TestInputAction>>(
        h.context_entity());
  }
  ~CtxSingleton() {
    EntityHelper::get_default_collection().singletonMap.erase(
        components::get_type_id<UIContext<ui_test::TestInputAction>>());
  }
};
}  // namespace

TEST(long_toast_wraps_within_width_and_grows_short_unchanged) {
  ui_test::ImmTestHarness h;
  CtxSingleton guard(h);
  const std::string long_msg =
      "The warehouse upload finished but three photos could not be "
      "matched to a room and need a person to review them today";
  auto notice = toast::send_info(h.context(), long_msg, 30.f);
  auto shorty = toast::send_info(h.context(), "Saved", 30.f);
  EntityHelper::merge_entity_arrays();
  toast::ToastLayoutSystem<ui_test::TestInputAction> layout;
  layout.once(0.f);
  CHECK(notice.ent().get<HasLabel>().text_overflow == TextOverflow::Wrap);
  const float base_h = toast::resolve_size(toast::HEIGHT, 800, 600);
  const float width = notice.cmp().rect().width;
  CHECK_APPROX(width, toast::resolve_size(toast::WIDTH, 800, 600));
  CHECK(notice.cmp().rect().height > base_h + 1.f);
  CHECK_APPROX(shorty.cmp().rect().height, base_h);
  const float font_px =
      toast::resolve_size(notice.cmp().font_size, 800, 600);
  const auto lines = wrap_text(notice.ent().get<HasLabel>().label,
                               width - 24.f,
                               notice.ent().get<HasLabel>().font_name, font_px);
  CHECK(lines.size() >= 2);
  for (const auto &line : lines)
    CHECK(measure_text_line(line, notice.ent().get<HasLabel>().font_name,
                            font_px)
              .x <=
          width - 24.f + 0.5f);
}

TEST(duplicate_toast_merges_and_refreshes_instead_of_stacking) {
  ui_test::ImmTestHarness h;
  auto first = toast::send_info(h.context(), "Synced", 30.f);
  EntityHelper::merge_entity_arrays();
  first.ent().get<toast::Toast>().elapsed = 2.f;
  const int before = live_toast_count();
  auto again = toast::send_info(h.context(), "Synced", 30.f);
  EntityHelper::merge_entity_arrays();
  CHECK(again.ent().id == first.ent().id);
  CHECK(live_toast_count() == before);
  CHECK_APPROX(first.ent().get<toast::Toast>().elapsed, 0.f);
}

TEST(toast_stack_is_capped_at_max_visible) {
  ui_test::ImmTestHarness h;
  for (int i = 0; i < toast::MAX_VISIBLE + 2; i++) {
    toast::send_info(h.context(), "Notice " + std::to_string(i), 30.f);
    EntityHelper::merge_entity_arrays();  // frame boundary between sends
  }
  CHECK(live_toast_count() == toast::MAX_VISIBLE);
}

TEST(hover_pauses_expiry_and_click_dismisses) {
  ui_test::ImmTestHarness h;
  CtxSingleton guard(h);
  auto notice = toast::send_info(h.context(), "Hover me", 1.f);
  EntityHelper::merge_entity_arrays();
  toast::ToastLayoutSystem<ui_test::TestInputAction> layout;
  layout.once(0.f);
  const auto rect = notice.cmp().rect();
  h.context().mouse.pos = {rect.x + rect.width / 2.f,
                           rect.y + rect.height / 2.f};
  h.context().mouse.just_pressed = false;
  layout.once(0.f);
  CHECK(notice.ent().get<toast::Toast>().hovered);
  toast::ToastUpdateSystem update;
  update.for_each_with(notice.ent(), notice.ent().get<toast::Toast>(), 5.f);
  CHECK(!notice.ent().get<toast::Toast>().is_expired());
  CHECK_APPROX(notice.ent().get<toast::Toast>().elapsed, 0.f);
  h.context().mouse.just_pressed = true;
  layout.once(0.f);
  CHECK(notice.ent().get<toast::Toast>().is_expired());
  h.context().mouse.just_pressed = false;
}

int main() { return ui_test::run_registered_tests("toast"); }
