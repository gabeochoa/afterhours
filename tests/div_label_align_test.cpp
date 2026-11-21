// div_label_align_test.cpp
// A Div's own label is not a flex child, so justify/align never moved it:
// with_align_items/justify_content looked dead and only with_alignment
// worked, undiscoverable from the container API. Contract: the own label
// reads the container's flex alignment (main = justify, cross = align),
// matching what child labels get from layout.
//
// Measured fixture: box 300x120 at (0,0), label "HI" at font size 20, so
// the text is 20x20 (harness measure stub) and the text inset is 0. Every
// expectation below is arithmetic from that, not read back off the code.

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
constexpr float BOX_W = 300.f;
constexpr float BOX_H = 120.f;
constexpr float TEXT = 20.f; // "HI" is 20 wide, 20 tall at font size 20

// The position of the recorded text draw for one div described by cfg.
Vector2Type text_pos(ComponentConfig cfg) {
  ImmTestHarness h;
  div(h.context(), mk(h.root(), 0),
      cfg.with_label("HI").with_font_size(pixels(TEXT)));
  h.render();
  for (const auto &call : h.drawn("text"))
    if (call.text == "HI")
      return {call.rect.x, call.rect.y};
  CHECK(false);
  return {-1.f, -1.f};
}

ComponentConfig box() {
  return ComponentConfig{}.with_size(
      ComponentSize{pixels(BOX_W), pixels(BOX_H)});
}
} // namespace

// Default: FlexStart on both axes, so top-left. Before this, the label sat
// at (0, 50): left horizontally but hard-centred vertically.
TEST(default_div_label_reads_flexstart) {
  const auto p = text_pos(box());
  CHECK_APPROX(p.x, 0.f);
  CHECK_APPROX(p.y, 0.f);
}

// Column: justify drives vertical, align drives horizontal.
TEST(column_div_label_follows_justify_and_align) {
  const JustifyContent jcs[] = {JustifyContent::FlexStart,
                                JustifyContent::Center, JustifyContent::FlexEnd};
  const float ys[] = {0.f, 50.f, 100.f};
  const AlignItems ais[] = {AlignItems::FlexStart, AlignItems::Center,
                            AlignItems::FlexEnd};
  const float xs[] = {0.f, 140.f, 280.f};
  for (int j = 0; j < 3; j++)
    for (int a = 0; a < 3; a++) {
      const auto p =
          text_pos(box().with_justify_content(jcs[j]).with_align_items(ais[a]));
      CHECK_APPROX(p.x, xs[a]);
      CHECK_APPROX(p.y, ys[j]);
    }
}

// Row: the axes swap; justify drives horizontal, align drives vertical.
TEST(row_div_label_follows_justify_and_align_swapped) {
  const JustifyContent jcs[] = {JustifyContent::FlexStart,
                                JustifyContent::Center, JustifyContent::FlexEnd};
  const float xs[] = {0.f, 140.f, 280.f};
  const AlignItems ais[] = {AlignItems::FlexStart, AlignItems::Center,
                            AlignItems::FlexEnd};
  const float ys[] = {0.f, 50.f, 100.f};
  for (int j = 0; j < 3; j++)
    for (int a = 0; a < 3; a++) {
      const auto p = text_pos(box()
                                  .with_flex_direction(FlexDirection::Row)
                                  .with_justify_content(jcs[j])
                                  .with_align_items(ais[a]));
      CHECK_APPROX(p.x, xs[j]);
      CHECK_APPROX(p.y, ys[a]);
    }
}

// A single text item packs at the start under SpaceBetween, centres under
// SpaceAround, and cannot stretch, so Stretch starts too.
TEST(single_item_distribution_values) {
  auto p = text_pos(box().with_justify_content(JustifyContent::SpaceBetween)
                        .with_align_items(AlignItems::Stretch));
  CHECK_APPROX(p.x, 0.f);
  CHECK_APPROX(p.y, 0.f);
  p = text_pos(box().with_justify_content(JustifyContent::SpaceAround));
  CHECK_APPROX(p.x, 0.f);
  CHECK_APPROX(p.y, 50.f);
}

// An explicit text alignment still wins horizontally (backwards compat for
// helpers like value_pill); vertical keeps following the flex props.
TEST(explicit_text_alignment_beats_flex_horizontally) {
  const auto p = text_pos(box()
                              .with_alignment(TextAlignment::Center)
                              .with_justify_content(JustifyContent::FlexEnd));
  CHECK_APPROX(p.x, 140.f);
  CHECK_APPROX(p.y, 100.f);
}

// Multi-line blocks follow the same vertical placement as single-line text.
TEST(multiline_own_label_follows_vertical) {
  ImmTestHarness h;
  div(h.context(), mk(h.root(), 0),
      box()
          .with_label("aaa\nbbb")
          .with_font_size(pixels(TEXT))
          .with_justify_content(JustifyContent::FlexEnd));
  h.render();
  const auto calls = h.drawn("text");
  CHECK(calls.size() == 2);
  if (calls.size() != 2)
    return;
  // Two 20px lines = 40px block at the bottom of 120.
  CHECK_APPROX(calls[0].rect.y, 80.f);
  CHECK_APPROX(calls[1].rect.y, 100.f);
}

// The batched renderer must land the label in the same place: it goes
// through the command buffer, which carries the vertical separately.
TEST(batched_renderer_honours_the_mapping) {
  auto batched_pos = [](ComponentConfig cfg) {
    ImmTestHarness h;
    div(h.context(), mk(h.root(), 0),
        cfg.with_label("HI").with_font_size(pixels(TEXT)));
    h.render_batched();
    for (const auto &call : h.drawn("text"))
      if (call.text == "HI")
        return Vector2Type{call.rect.x, call.rect.y};
    CHECK(false);
    return Vector2Type{-1.f, -1.f};
  };
  auto p = batched_pos(box());
  CHECK_APPROX(p.x, 0.f);
  CHECK_APPROX(p.y, 0.f);
  p = batched_pos(box()
                      .with_justify_content(JustifyContent::Center)
                      .with_align_items(AlignItems::Center));
  CHECK_APPROX(p.x, 140.f);
  CHECK_APPROX(p.y, 50.f);
  p = batched_pos(box()
                      .with_flex_direction(FlexDirection::Row)
                      .with_justify_content(JustifyContent::FlexEnd)
                      .with_align_items(AlignItems::FlexEnd));
  CHECK_APPROX(p.x, 280.f);
  CHECK_APPROX(p.y, 100.f);
}

int main() { return ui_test::run_registered_tests("div label align"); }
