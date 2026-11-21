#include "ui_test_harness.h"
using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;
namespace {
constexpr float BOX_W = 300.f;
constexpr float BOX_H = 120.f;
constexpr float TEXT = 20.f;
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
}

TEST(default_div_label_reads_flexstart) {
  const auto p = text_pos(box());
  CHECK_APPROX(p.x, 0.f);
  CHECK_APPROX(p.y, 0.f);
}

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

TEST(single_item_distribution_values) {
  auto p = text_pos(box().with_justify_content(JustifyContent::SpaceBetween)
                        .with_align_items(AlignItems::Stretch));
  CHECK_APPROX(p.x, 0.f);
  CHECK_APPROX(p.y, 0.f);
  p = text_pos(box().with_justify_content(JustifyContent::SpaceAround));
  CHECK_APPROX(p.x, 0.f);
  CHECK_APPROX(p.y, 50.f);
}

TEST(explicit_text_alignment_beats_flex_horizontally) {
  const auto p = text_pos(box()
                              .with_alignment(TextAlignment::Center)
                              .with_justify_content(JustifyContent::FlexEnd));
  CHECK_APPROX(p.x, 140.f);
  CHECK_APPROX(p.y, 100.f);
}

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
  CHECK_APPROX(calls[0].rect.y, 80.f);
  CHECK_APPROX(calls[1].rect.y, 100.f);
}

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
