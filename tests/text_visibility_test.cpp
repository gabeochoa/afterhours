#define AFTER_HOURS_ENABLE_E2E_TESTING
#include "ui_test_harness.h"
#include <afterhours/src/graphics.h>
#include <afterhours/src/plugins/e2e_testing/command_handlers.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using namespace afterhours::testing;

static int render_label(bool batched, float clip_height, bool styled,
                        float scroll_offset = 0.f, float text_offset = 0.f) {
  ui_test::ImmTestHarness h;
  h.begin_frame();
  auto outer = div(h.context(), mk(h.root(), 0), ComponentConfig{}
      .with_size({pixels(180), pixels(clip_height)})
      .with_absolute_position(20, 20).with_transparent_bg()
      .with_overflow(Overflow::Hidden));
  auto inner = div(h.context(), mk(outer.ent(), 0), ComponentConfig{}
      .with_size({pixels(180), pixels(90)})
      .with_absolute_position(0, 0).with_transparent_bg()
      .with_overflow(Overflow::Scroll, Axis::Y));
  div(h.context(), mk(inner.ent(), 1), ComponentConfig{}
      .with_size({pixels(180), pixels(300)}).with_absolute_position(0, 0)
      .with_transparent_bg());
  auto style = ComponentConfig{}.with_size({pixels(180), pixels(24)})
      .with_absolute_position(0, 0).with_transparent_bg()
      .with_font(UIComponent::DEFAULT_FONT, pixels(24))
      .with_text_inset(0).with_alignment(TextAlignment::Left)
      .with_custom_text_color({255, 255, 255, 255});
  if (styled)
    style.with_styled_label({TextSpan{"Clip ", Color{255, 255, 255, 255}},
                             TextSpan{"probe", Color{255, 255, 255, 255}}});
  else
    style.with_label("Clip probe");
  auto label = div(h.context(), mk(inner.ent(), 0), style);
  label.ent().get<HasLabel>().text_y_offset = text_offset;
  h.layout_only();
  auto &scroll = inner.ent().get<HasScrollView>();
  scroll.viewport_size = {180, 90};
  scroll.content_size = {180, 300};
  scroll.scroll_offset.y = scroll_offset;
  auto &registry = VisibleTextRegistry::instance();
  registry.clear();
  FontManager fonts;
  fonts.load_font(UIComponent::DEFAULT_FONT, get_default_font());
  fonts.load_font(UIComponent::UNSET_FONT, get_default_font());
  graphics::begin_frame();
  graphics::clear_background(Color{0, 0, 0, 255});
  if (batched)
    RenderBatched<ui_test::TestInputAction>{}.for_each_with_derived(h.root(), h.context(), fonts, 0.f);
  else
    RenderImm<ui_test::TestInputAction>{}.for_each_with_derived(h.root(), h.context(), fonts, 0.f);
  graphics::end_frame();
  CHECK(ui_commands::check_ui_property(label.ent(), "text", "Clip probe").empty());
  const auto capture = capture_render_texture_rgba(graphics::get_render_texture());
  CHECK(capture.has_value());
  if (!capture) return 0;
  int pixels = 0;
  for (int y = 0; y < 130; ++y) {
    for (int x = 0; x < 180; ++x) {
      const size_t index = (static_cast<size_t>(y) * capture->width + x) * 4;
      if (capture->pixels[index] > 200 && capture->pixels[index + 1] > 200 &&
          capture->pixels[index + 2] > 200) ++pixels;
    }
  }
  return pixels;
}

TEST(rendered_pixels_match_nested_clip_visibility) {
  test_input::detail::test_mode = true;
  auto &registry = VisibleTextRegistry::instance();
  for (bool batched : {false, true}) {
    for (bool styled : {false, true}) {
      const int full = render_label(batched, 50, styled);
      CHECK(full > 0);
      CHECK(registry.contains("Clip probe"));
      CHECK(registry.contains_fully_visible("Clip probe"));
      const int partial = render_label(batched, 12, styled);
      CHECK(partial > 0 && partial < full);
      CHECK(registry.contains("Clip probe"));
      CHECK(!registry.contains_fully_visible("Clip probe"));
      CHECK(render_label(batched, 50, styled, 0, 60) == 0);
      CHECK(!registry.contains("Clip probe"));
      CHECK(!registry.contains_fully_visible("Clip probe"));
      CHECK(render_label(batched, 50, styled, 100) == 0);
      CHECK(!registry.contains("Clip probe"));
      CHECK(!registry.contains_fully_visible("Clip probe"));
    }
  }
  test_input::detail::test_mode = false;
}

TEST(multiline_registration_does_not_bypass_ancestor_clips) {
  test_input::detail::test_mode = true;
  for (bool batched : {false, true}) {
    ui_test::ImmTestHarness h;
    h.begin_frame();
    auto clip = div(h.context(), mk(h.root(), 0), ComponentConfig{}
        .with_size({pixels(180), pixels(24)}).with_absolute_position(20, 20)
        .with_transparent_bg().with_overflow(Overflow::Hidden));
    div(h.context(), mk(clip.ent(), 0), ComponentConfig{}
        .with_size({pixels(180), pixels(48)}).with_absolute_position(0, 0)
        .with_label("First line\nSecond line").with_transparent_bg()
        .with_font(UIComponent::DEFAULT_FONT, pixels(24)).with_text_inset(0));
    h.layout_only();
    auto &registry = VisibleTextRegistry::instance();
    registry.clear();
    FontManager fonts;
    fonts.load_font(UIComponent::DEFAULT_FONT, get_default_font());
    fonts.load_font(UIComponent::UNSET_FONT, get_default_font());
    graphics::begin_frame();
    graphics::clear_background(Color{0, 0, 0, 255});
    if (batched)
      RenderBatched<ui_test::TestInputAction>{}.for_each_with_derived(h.root(), h.context(), fonts, 0.f);
    else
      RenderImm<ui_test::TestInputAction>{}.for_each_with_derived(h.root(), h.context(), fonts, 0.f);
    graphics::end_frame();
    CHECK(registry.contains("First line\nSecond line"));
    CHECK(!registry.contains_fully_visible("First line"));
    CHECK(!registry.contains_fully_visible("Second line"));
    CHECK(registry.get_texts().size() == 1);
  }
  test_input::detail::test_mode = false;
}

TEST(registry_and_commands_distinguish_partial_full_and_absent) {
  auto &registry = VisibleTextRegistry::instance();
  registry.clear();
  registry.register_text_in_clip("partial", 0, 0, 100, 20, 10, 0, 90, 20);
  registry.register_text_if_visible("full", 10, 10, 100, 20, 800, 600);
  registry.register_text_if_visible("edge", -99.5f, 0, 100, 20, 800, 600);
  registry.register_text_if_visible("outside", 800, 0, 100, 20, 800, 600);
  registry.register_text_in_clip("empty", 0, 0, 0, 20, 0, 0, 800, 600);
  CHECK(registry.contains("partial"));
  CHECK(!registry.contains_fully_visible("partial"));
  CHECK(registry.contains_fully_visible("full"));
  CHECK(!registry.contains("edge"));
  CHECK(!registry.contains("outside"));
  CHECK(!registry.contains("empty"));
  ui_test::ImmTestHarness h;
  for (const std::string name : {"expect_text", "expect_text_fully_visible"}) {
    for (const std::string text : {"partial", "full", "absent"}) {
      PendingE2ECommand cmd;
      cmd.name = name;
      cmd.args = {text};
      HandleExpectTextCommand{}.for_each_with(h.root(), cmd, 0.f);
      const bool expected = text == "full" || (text == "partial" && name == "expect_text");
      CHECK(cmd.is_consumed() == expected);
      CHECK(cmd.error_message.empty());
    }
  }
  const auto generation = registry.generation();
  registry.clear();
  CHECK(registry.generation() == generation + 1);
  CHECK(!registry.contains_fully_visible("full"));
  registry.register_text("custom draw");
  CHECK(registry.contains_fully_visible("custom draw"));
}

int main() {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 800;
  config.height = 600;
  if (!graphics::init(config)) return 1;
  const int result = ui_test::run_registered_tests("text visibility");
  graphics::shutdown();
  return result;
}
