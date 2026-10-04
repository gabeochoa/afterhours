// color_swatch_test.cpp
// The inline variant: a swatch button (chip + hex) that expands into
// H/S/V/A sliders and a hex field. Preset and popover variants extend
// this file when they land.

#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/color_swatch.h>

#include <string>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
int count_named(const std::string &prefix) {
  int n = 0;
  for (const auto &e : UICollectionHolder::get().collection.get_entities()) {
    if (e && e->has<UIComponentDebug>() &&
        e->get<UIComponentDebug>().name().rfind(prefix, 0) == 0)
      n++;
  }
  return n;
}

ComponentConfig swatch_cfg(const std::string &name) {
  return ComponentConfig{}
      .with_size(ComponentSize{pixels(180), pixels(28)})
      .with_debug_name(name);
}
} // namespace

TEST(swatch_button_shows_the_hex_value) {
  ImmTestHarness h;
  h.begin_frame();
  color_swatch_button(h.context(), mk(h.root(), 0), Color{60, 101, 156, 255},
                      swatch_cfg("accent_swatch"));
  h.layout_only();

  auto *btn = h.find("accent_swatch");
  ui_test::check(btn != nullptr, "the swatch exists", __FILE__, __LINE__);
  if (!btn)
    return;
  auto entity = UICollectionHolder::getEntityForID(btn->id);
  ui_test::check(entity->get<HasLabel>().label == "#3C659C",
                 "the button label is the hex value", __FILE__, __LINE__);
  ui_test::check(count_named("color_chip") == 1, "one chip is built", __FILE__,
                 __LINE__);
}

TEST(inline_swatch_closed_builds_no_sliders) {
  ImmTestHarness h;
  Color value{60, 101, 156, 255};
  bool open = false;
  h.begin_frame();
  color_swatch_inline(h.context(), mk(h.root(), 0), value, open,
                      swatch_cfg("accent_inline"));
  h.layout_only();
  ui_test::check(count_named("color_channel_") == 0,
                 "a closed swatch builds no channel rows", __FILE__, __LINE__);
  ui_test::check(count_named("color_hex_field") == 0,
                 "a closed swatch builds no hex field", __FILE__, __LINE__);
}

TEST(inline_swatch_open_builds_channels_and_hex) {
  ImmTestHarness h;
  Color value{60, 101, 156, 255};
  bool open = true;
  auto emit = [&] {
    return color_swatch_inline(h.context(), mk(h.root(), 0), value, open,
                               swatch_cfg("accent_inline"));
  };
  h.begin_frame(); emit(); h.layout_only();
  h.begin_frame(); emit(); h.layout_only();

  ui_test::check(count_named("color_channel_row") == 4,
                 "H, S, V and A rows are built", __FILE__, __LINE__);
  for (const char *name :
       {"color_channel_h", "color_channel_s", "color_channel_v",
        "color_channel_a"})
    ui_test::check(h.find(name) != nullptr, "each channel slider exists",
                   __FILE__, __LINE__);
  ui_test::check(count_named("color_hex_field") == 1, "the hex field exists",
                 __FILE__, __LINE__);
  ui_test::check(value.r == 60 && value.g == 101 && value.b == 156 &&
                     value.a == 255,
                 "building does not change the value", __FILE__, __LINE__);
}

TEST(channel_slider_handle_is_sized_and_inside_the_track) {
  ImmTestHarness h;
  Color value{60, 101, 156, 255};
  bool open = true;
  auto emit = [&] {
    return color_swatch_inline(h.context(), mk(h.root(), 0), value, open,
                               ComponentConfig{}
                                   .with_size(ComponentSize{pixels(368),
                                                            pixels(36)})
                                   .with_debug_name("accent_inline"));
  };
  h.begin_frame(); emit(); h.layout_only();
  h.begin_frame(); emit(); h.layout_only();

  const UIComponent *track = h.find("slider_background");
  const UIComponent *handle = h.find("slider_handle");
  ui_test::check(track != nullptr && handle != nullptr,
                 "channel slider track and handle exist", __FILE__, __LINE__);
  if (track && handle) {
    const RectangleType tr = track->rect();
    const RectangleType hr = handle->rect();
    ui_test::check(hr.width > 4.f && hr.height > 4.f,
                   "the handle has a size", __FILE__, __LINE__);
    ui_test::check(hr.x >= tr.x - 1.f &&
                       hr.x + hr.width <= tr.x + tr.width + 1.f,
                   "the handle sits inside the track", __FILE__, __LINE__);
  }
}

int main() { return ui_test::run_registered_tests("color_swatch"); }
