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

namespace {
constexpr std::array<Color, 4> test_presets{{
    {60, 101, 156, 255},
    {46, 125, 107, 255},
    {176, 58, 72, 255},
    {255, 213, 79, 255},
}};
} // namespace

TEST(preset_popover_closed_builds_no_grid) {
  ImmTestHarness h;
  Color value{60, 101, 156, 255};
  bool open = false;
  h.begin_frame();
  color_swatch_presets(h.context(), mk(h.root(), 0), value, open, test_presets,
                       swatch_cfg("accent_presets"));
  h.layout_only();
  ui_test::check(count_named("color_preset_") == 0,
                 "a closed preset swatch builds no chips", __FILE__, __LINE__);
  ui_test::check(count_named("color_hex_field") == 0,
                 "a closed preset swatch builds no hex field", __FILE__,
                 __LINE__);
}

TEST(preset_popover_open_builds_grid_and_hex) {
  ImmTestHarness h;
  Color value{60, 101, 156, 255};
  bool open = true;
  auto emit = [&] {
    return color_swatch_presets(h.context(), mk(h.root(), 0), value, open,
                                test_presets, swatch_cfg("accent_presets"));
  };
  h.begin_frame(); emit(); h.layout_only();
  h.begin_frame(); emit(); h.layout_only();

  ui_test::check(open, "the popover stays open once it holds focus", __FILE__,
                 __LINE__);
  for (const char *name : {"color_preset_0", "color_preset_1",
                           "color_preset_2", "color_preset_3"})
    ui_test::check(h.find(name) != nullptr, "each preset chip is built",
                   __FILE__, __LINE__);
  ui_test::check(count_named("color_hex_field") == 1,
                 "the custom hex field is built under the grid", __FILE__,
                 __LINE__);
  const UIComponent *panel = h.find("popover_panel");
  const UIComponent *chip = h.find("color_preset_3");
  ui_test::check(panel != nullptr && chip != nullptr,
                 "the popover panel and last chip exist", __FILE__, __LINE__);
  if (panel && chip) {
    const RectangleType pr = panel->rect();
    const RectangleType cr = chip->rect();
    ui_test::check(cr.x >= pr.x - 1.f && cr.y >= pr.y - 1.f &&
                       cr.x + cr.width <= pr.x + pr.width + 1.f &&
                       cr.y + cr.height <= pr.y + pr.height + 1.f,
                   "the grid fits inside the sized popover panel", __FILE__,
                   __LINE__);
  }
  ui_test::check(value.r == 60 && value.g == 101 && value.b == 156 &&
                     value.a == 255,
                 "building does not change the value", __FILE__, __LINE__);
}

TEST(preset_hover_previews_on_the_anchor_without_publishing) {
  ImmTestHarness h;
  Color value{60, 101, 156, 255};
  bool open = true;
  auto emit = [&] {
    return color_swatch_presets(h.context(), mk(h.root(), 0), value, open,
                                test_presets, swatch_cfg("accent_presets"));
  };
  h.begin_frame(); emit(); h.layout_only();
  h.begin_frame(); emit(); h.layout_only();

  const UIComponent *chip = h.find("color_preset_1");
  ui_test::check(chip != nullptr, "the preset chip to hover exists", __FILE__,
                 __LINE__);
  if (!chip)
    return;
  // The hover background must differ from the preset or hovering the
  // palette changes nothing on screen.
  auto chip_entity = UICollectionHolder::getEntityForID(chip->id);
  ui_test::check(chip_entity->has<HasColor>(), "the chip carries a colour",
                 __FILE__, __LINE__);
  if (chip_entity->has<HasColor>()) {
    const auto &hc = chip_entity->get<HasColor>();
    ui_test::check(hc.hover_color.has_value() &&
                       !color_swatch_detail::same_color(
                           hc.hover_color.value(), test_presets[1]),
                   "the chip hover background differs from the preset",
                   __FILE__, __LINE__);
  }

  // Leave the chip hot the way the previous frame's hit test would, then
  // build twice: one frame to record the preview, one to show it on the
  // anchor that is built before the panel.
  const EntityID chip_id = chip->id;
  h.context().prev_hot_id = chip_id;
  h.begin_frame(); auto result = emit(); h.layout_only();
  h.begin_frame(); emit(); h.layout_only();

  auto &state = result.ent().template get<HasColorSwatchState>();
  ui_test::check(state.hover_preview.has_value() &&
                     color_swatch_detail::same_color(
                         state.hover_preview.value(), test_presets[1]),
                 "hovering records the preset as a preview", __FILE__,
                 __LINE__);
  ui_test::check(color_swatch_detail::same_color(value, test_presets[0]),
                 "hovering does not publish the preset", __FILE__, __LINE__);
  const UIComponent *btn = h.find("accent_presets");
  ui_test::check(btn != nullptr, "the anchor exists", __FILE__, __LINE__);
  if (btn) {
    auto btn_entity = UICollectionHolder::getEntityForID(btn->id);
    ui_test::check(btn_entity->get<HasLabel>().label == "#2E7D6B",
                   "the anchor shows the previewed hex", __FILE__, __LINE__);
  }

  // Move the pointer away: the preview clears and the anchor is back to
  // the saved value, still unpublished.
  h.context().prev_hot_id = h.root().id;
  h.begin_frame(); emit(); h.layout_only();
  h.begin_frame(); emit(); h.layout_only();
  ui_test::check(!state.hover_preview.has_value(),
                 "moving away clears the preview", __FILE__, __LINE__);
  ui_test::check(color_swatch_detail::same_color(value, test_presets[0]),
                 "the value is still the saved one", __FILE__, __LINE__);
  const UIComponent *btn2 = h.find("accent_presets");
  ui_test::check(btn2 != nullptr, "the anchor still exists", __FILE__,
                 __LINE__);
  if (btn2) {
    auto btn_entity = UICollectionHolder::getEntityForID(btn2->id);
    ui_test::check(btn_entity->get<HasLabel>().label == "#3C659C",
                   "the anchor is back to the saved hex", __FILE__,
                   __LINE__);
  }
}

TEST(editor_popover_closed_builds_no_editor) {
  ImmTestHarness h;
  Color value{60, 101, 156, 255};
  bool open = false;
  h.begin_frame();
  color_swatch_editor(h.context(), mk(h.root(), 0), value, open,
                      swatch_cfg("accent_editor"));
  h.layout_only();
  ui_test::check(count_named("color_sv_square") == 0,
                 "a closed editor builds no SV square", __FILE__, __LINE__);
  ui_test::check(count_named("color_hue_strip") == 0,
                 "a closed editor builds no hue strip", __FILE__, __LINE__);
  ui_test::check(count_named("color_alpha_strip") == 0,
                 "a closed editor builds no alpha strip", __FILE__, __LINE__);
  ui_test::check(count_named("color_hex_field") == 0,
                 "a closed editor builds no hex field", __FILE__, __LINE__);
}

TEST(editor_popover_open_builds_square_strips_hex_and_compare) {
  ImmTestHarness h;
  Color value{60, 101, 156, 255};
  bool open = true;
  auto emit = [&] {
    return color_swatch_editor(h.context(), mk(h.root(), 0), value, open,
                               swatch_cfg("accent_editor"));
  };
  h.begin_frame(); emit(); h.layout_only();
  h.begin_frame(); emit(); h.layout_only();

  ui_test::check(open, "the editor stays open once it holds focus", __FILE__,
                 __LINE__);
  for (const char *name : {"color_sv_square", "color_hue_strip",
                           "color_alpha_strip", "color_editor_before",
                           "color_editor_after"})
    ui_test::check(h.find(name) != nullptr, "each editor part is built",
                   __FILE__, __LINE__);
  ui_test::check(count_named("color_hex_field") == 1,
                 "the hex field is built under the strips", __FILE__,
                 __LINE__);
  const UIComponent *panel = h.find("popover_panel");
  const UIComponent *square = h.find("color_sv_square");
  const UIComponent *after = h.find("color_editor_after");
  ui_test::check(panel != nullptr && square != nullptr && after != nullptr,
                 "the panel, square and compare chip exist", __FILE__,
                 __LINE__);
  if (panel && square && after) {
    const RectangleType pr = panel->rect();
    const RectangleType sr = square->rect();
    const RectangleType ar = after->rect();
    ui_test::check(sr.width > 40.f && sr.height > 20.f,
                   "the SV square has a drag surface", __FILE__, __LINE__);
    ui_test::check(sr.x >= pr.x - 1.f && sr.y >= pr.y - 1.f &&
                       ar.x + ar.width <= pr.x + pr.width + 1.f &&
                       ar.y + ar.height <= pr.y + pr.height + 1.f,
                   "the editor fits inside the sized popover panel",
                   __FILE__, __LINE__);
  }
  ui_test::check(color_swatch_detail::same_color(value, test_presets[0]),
                 "building does not change the value", __FILE__, __LINE__);
}

TEST(editor_pending_edits_publish_on_the_next_build) {
  ImmTestHarness h;
  Color value{60, 101, 156, 255};
  bool open = true;
  auto emit = [&] {
    return color_swatch_editor(h.context(), mk(h.root(), 0), value, open,
                               swatch_cfg("accent_editor"));
  };
  h.begin_frame(); emit(); h.layout_only();
  h.begin_frame(); auto result = emit(); h.layout_only();

  const UIComponent *hue_strip = h.find("color_hue_strip");
  ui_test::check(hue_strip != nullptr, "the hue strip exists", __FILE__,
                 __LINE__);
  if (!hue_strip)
    return;
  auto strip_entity = UICollectionHolder::getEntityForID(hue_strip->id);
  ui_test::check(strip_entity->has<HasLeftRightListener>(),
                 "the hue strip takes arrow keys", __FILE__, __LINE__);
  if (strip_entity->has<HasLeftRightListener>())
    strip_entity->get<HasLeftRightListener>().cb(strip_entity.asE(), 1);

  ui_test::check(color_swatch_detail::same_color(value, test_presets[0]),
                 "a drag or key edit lands in state first", __FILE__,
                 __LINE__);
  h.begin_frame(); emit(); h.layout_only();
  colors::HSV expected_hsv = colors::to_hsv(test_presets[0]);
  expected_hsv.h += 1.f;
  const Color expected = colors::from_hsv(expected_hsv, 255);
  ui_test::check(color_swatch_detail::same_color(value, expected),
                 "the next build publishes the hue edit", __FILE__,
                 __LINE__);

  auto &state = result.ent().template get<HasColorSwatchState>();
  state.editor_value = Color{255, 0, 0, 128};
  state.editor_changed = true;
  h.begin_frame(); emit(); h.layout_only();
  h.begin_frame(); emit(); h.layout_only();
  ui_test::check(
      color_swatch_detail::same_color(value, Color{255, 0, 0, 128}),
      "a square drag publishes through the same pending state", __FILE__,
      __LINE__);
  const UIComponent *before = h.find("color_editor_before");
  ui_test::check(before != nullptr, "the before chip exists", __FILE__,
                 __LINE__);
  if (before) {
    auto before_entity = UICollectionHolder::getEntityForID(before->id);
    ui_test::check(before_entity->has<HasColor>() &&
                       color_swatch_detail::same_color(
                           before_entity->get<HasColor>().color(),
                           test_presets[0]),
                   "the before chip still shows the opening colour",
                   __FILE__, __LINE__);
  }
}

int main() { return ui_test::run_registered_tests("color_swatch"); }
