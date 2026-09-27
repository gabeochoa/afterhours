// pseudo_locale_test.cpp
// Pseudo-localization for layout stress: built-in fake languages that make
// every label longer (DoubleWords) or run its words in reverse (RtlWords),
// applied where labels are built so measurement sees the stressed text.

#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/pseudo_locale.h>
#include <afterhours/src/plugins/ui/styling_defaults.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

TEST(double_words_doubles_each_line) {
  CHECK(pseudo_localize("Save changes", PseudoLocale::DoubleWords) ==
        "Save changes Save changes");
  CHECK(pseudo_localize("Save", PseudoLocale::DoubleWords) == "Save Save");
  CHECK(pseudo_localize("", PseudoLocale::DoubleWords).empty());
  CHECK(pseudo_localize("a b\nc d", PseudoLocale::DoubleWords) ==
        "a b a b\nc d c d");
}

TEST(rtl_words_reverses_word_order_per_line) {
  CHECK(pseudo_localize("the quick brown fox", PseudoLocale::RtlWords) ==
        "fox brown quick the");
  CHECK(pseudo_localize("Save", PseudoLocale::RtlWords) == "Save");
  CHECK(pseudo_localize("", PseudoLocale::RtlWords).empty());
  CHECK(pseudo_localize("one two\nthree four", PseudoLocale::RtlWords) ==
        "two one\nfour three");
}

TEST(none_mode_is_identity) {
  CHECK(pseudo_localize("Save changes", PseudoLocale::None) == "Save changes");
  CHECK(pseudo_localize("a  b", PseudoLocale::None) == "a  b");
}

TEST(spans_transform_with_their_text) {
  const std::vector<TextSpan> spans{{"Save ", Color{255, 0, 0, 255}},
                                    {"changes", Color{0, 255, 0, 255}}};
  const auto doubled = pseudo_localize_spans(spans, PseudoLocale::DoubleWords);
  CHECK(doubled.size() == 4);
  if (doubled.size() == 4) {
    CHECK(doubled[0].text == "Save ");
    CHECK(doubled[2].text == " Save ");
    CHECK(doubled[3].text == "changes");
    CHECK(doubled[3].color.g == 255);
  }
  const auto rtl = pseudo_localize_spans(spans, PseudoLocale::RtlWords);
  CHECK(rtl.size() == 2);
  if (rtl.size() == 2) {
    CHECK(rtl[0].text == "changes");
    // The space that separated the runs moves to the other side of its run.
    CHECK(rtl[1].text == " Save");
  }
  CHECK(pseudo_localize_spans(spans, PseudoLocale::None) == spans);
  CHECK(pseudo_localize_spans({}, PseudoLocale::DoubleWords).empty());
}

namespace {
struct PseudoGuard {
  explicit PseudoGuard(PseudoLocale mode) {
    UIStylingDefaults::get().set_pseudo_locale(mode);
  }
  ~PseudoGuard() { UIStylingDefaults::get().set_pseudo_locale(PseudoLocale::None); }
};
} // namespace

TEST(labels_are_built_in_the_active_pseudo_locale) {
  PseudoGuard guard(PseudoLocale::DoubleWords);
  ui_test::ImmTestHarness h;
  auto result = button(h.context(), mk(h.root(), 0),
                       ComponentConfig{}.with_label("Save changes"));
  h.layout_only();
  CHECK(result.ent().get<HasLabel>().label == "Save changes Save changes");
}

TEST(rtl_labels_default_to_right_alignment_unless_told) {
  PseudoGuard guard(PseudoLocale::RtlWords);
  ui_test::ImmTestHarness h;
  auto plain = div(h.context(), mk(h.root(), 0),
                   ComponentConfig{}.with_label("the quick fox"));
  auto pinned = div(h.context(), mk(h.root(), 1),
                    ComponentConfig{}
                        .with_label("the quick fox")
                        .with_alignment(TextAlignment::Left));
  h.layout_only();
  CHECK(plain.ent().get<HasLabel>().label == "fox quick the");
  CHECK(plain.ent().get<HasLabel>().alignment == TextAlignment::Right);
  CHECK(pinned.ent().get<HasLabel>().alignment == TextAlignment::Left);
}

// In RTL the start side is the right: asymmetric horizontal padding and
// margin swap, and a row lays its children out in reverse, so a label
// that sat left of its button sits right of it.
TEST(rtl_mirroring_swaps_start_padding_and_margin) {
  PseudoGuard guard(PseudoLocale::RtlWords);
  ui_test::ImmTestHarness h;
  auto result = div(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size({pixels(200), pixels(50)})
                        .with_padding(Padding{.left = pixels(30),
                                              .right = pixels(4)})
                        .with_margin(Margin{.left = pixels(9),
                                            .right = pixels(1)}));
  h.layout_only();
  const auto &cmp = result.ent().get<UIComponent>();
  CHECK_APPROX(cmp.desired_padding[Axis::left].value, 4.f);
  CHECK_APPROX(cmp.desired_padding[Axis::right].value, 30.f);
  CHECK_APPROX(cmp.desired_margin[Axis::left].value, 1.f);
  CHECK_APPROX(cmp.desired_margin[Axis::right].value, 9.f);
}

TEST(ltr_keeps_start_padding_and_margin) {
  ui_test::ImmTestHarness h;
  auto result = div(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size({pixels(200), pixels(50)})
                        .with_padding(Padding{.left = pixels(30),
                                              .right = pixels(4)})
                        .with_margin(Margin{.left = pixels(9),
                                            .right = pixels(1)}));
  h.layout_only();
  const auto &cmp = result.ent().get<UIComponent>();
  CHECK_APPROX(cmp.desired_padding[Axis::left].value, 30.f);
  CHECK_APPROX(cmp.desired_padding[Axis::right].value, 4.f);
  CHECK_APPROX(cmp.desired_margin[Axis::left].value, 9.f);
}

TEST(rtl_rows_put_the_label_on_the_other_side) {
  auto child_xs = [](PseudoLocale mode) {
    PseudoGuard guard(mode);
    ui_test::ImmTestHarness h;
    auto row = hstack(h.context(), mk(h.root(), 0),
                      ComponentConfig{}.with_size({pixels(300), pixels(50)}));
    auto a = div(h.context(), mk(row.ent(), 0),
                 ComponentConfig{}.with_size({pixels(50), pixels(50)}));
    auto b = div(h.context(), mk(row.ent(), 1),
                 ComponentConfig{}.with_size({pixels(50), pixels(50)}));
    h.layout_only();
    return std::pair{a.ent().get<UIComponent>().rect().x,
                     b.ent().get<UIComponent>().rect().x};
  };
  const auto ltr = child_xs(PseudoLocale::None);
  CHECK(ltr.first < ltr.second);
  const auto rtl = child_xs(PseudoLocale::RtlWords);
  CHECK(rtl.first > rtl.second);
}

int main() { return ui_test::run_registered_tests("pseudo locale"); }
