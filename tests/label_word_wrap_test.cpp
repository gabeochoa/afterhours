// label_word_wrap_test.cpp
// with_word_wrap() on a plain label (not just a text_area).
//
// The wrap renderer, measurer and helpers all key off TextOverflow::Wrap, but
// with_word_wrap() only set the text-area flag, so calling it on a label
// changed nothing: a long hint stayed one line and was clipped. The probe
// that pinned this down measured a 43-char label in a 200px box at h=20
// (one line) via with_word_wrap(true) and h=60 (three lines) via
// with_text_overflow(TextOverflow::Wrap).
//
// Runs under the RECORDING backend; harness measure is chars * font_size *
// 0.5 wide, font_size tall, so at FS=20 each char is 10px and each line 20px.

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
constexpr float FS = 20.f;  // font size used by every test here
constexpr float LINE_H = FS;
constexpr const char *kPara = "the quick brown fox jumps over the lazy dog";

Rectangle rect_of(const ElementResult &e) {
  return e.ent().get<UIComponent>().rect();
}
} // namespace

// A long label in a fixed-width box wraps through with_word_wrap(true),
// reaching the same height as TextOverflow::Wrap.
TEST(word_wrap_wraps_a_plain_label) {
  ImmTestHarness h;
  auto via_word_wrap = div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(200),
                                   Size{Dim::Text, 0.f, 1.f}})
          .with_label(kPara)
          .with_word_wrap(true)
          .with_font_size(FS));
  auto via_overflow = div(h.context(), mk(h.root(), 1),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(200),
                                   Size{Dim::Text, 0.f, 1.f}})
          .with_label(kPara)
          .with_text_overflow(TextOverflow::Wrap)
          .with_font_size(FS));
  h.layout_only();

  CHECK(rect_of(via_word_wrap).height > LINE_H);
  CHECK_APPROX(rect_of(via_word_wrap).height,
               rect_of(via_overflow).height);
  CHECK_APPROX(rect_of(via_word_wrap).height, 3.f * LINE_H);
  CHECK(rect_of(via_word_wrap).width <= 200.f);
}

// A label that fits on one line is unchanged by with_word_wrap(true).
TEST(word_wrap_leaves_a_short_label_on_one_line) {
  ImmTestHarness h;
  auto wrapped = div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(200),
                                   Size{Dim::Text, 0.f, 1.f}})
          .with_label("fits")
          .with_word_wrap(true)
          .with_font_size(FS));
  auto plain = div(h.context(), mk(h.root(), 1),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(200),
                                   Size{Dim::Text, 0.f, 1.f}})
          .with_label("fits")
          .with_font_size(FS));
  h.layout_only();

  CHECK_APPROX(rect_of(wrapped).height, LINE_H);
  CHECK_APPROX(rect_of(wrapped).width, rect_of(plain).width);
}

// with_word_wrap(false) keeps the label on one line.
TEST(word_wrap_false_keeps_one_line) {
  ImmTestHarness h;
  auto e = div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(200),
                                   Size{Dim::Text, 0.f, 1.f}})
          .with_label(kPara)
          .with_word_wrap(false)
          .with_font_size(FS));
  h.layout_only();

  CHECK_APPROX(rect_of(e).height, LINE_H);
}

int main() { return ui_test::run_registered_tests("label word wrap tests"); }
