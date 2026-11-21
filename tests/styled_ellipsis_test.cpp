
#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/text_selection.h>
#include <afterhours/src/plugins/ui/text_units.h>

using afterhours::Color;
using afterhours::colors::FontWeight;
using afterhours::ui::TextSpan;

namespace {
constexpr float CHAR_W = 10.f;

float measure_w(const std::string &s, FontWeight) {
  return static_cast<float>(s.size()) * CHAR_W;
}

const Color kWhite{255, 255, 255, 255};
const Color kRed{255, 0, 0, 255};
const Color kBlue{0, 0, 255, 255};

std::string joined(const std::vector<TextSpan> &runs) {
  std::string out;
  for (const auto &run : runs)
    out += run.text;
  return out;
}

float drawn_width(const std::vector<TextSpan> &runs) {
  float w = 0.f;
  for (const auto &run : runs)
    w += measure_w(run.text, run.weight);
  return w;
}
}

TEST(fits_returned_unchanged) {
  const std::vector<TextSpan> runs{{"hi ", kRed}, {"there", kWhite}};
  const auto out = afterhours::ui::detail::ellipsize_runs(runs, 1000.f, measure_w);
  CHECK(out == runs);
  CHECK(joined(out) == "hi there");
}

TEST(single_run_matches_plain_ellipsis) {
  const std::vector<TextSpan> runs{{"hello world", kWhite}};
  const auto out =
      afterhours::ui::detail::ellipsize_runs(runs, 70.f, measure_w);
  CHECK(joined(out) == "hell...");
  CHECK(drawn_width(out) <= 70.f);
  CHECK(joined(out) ==
        afterhours::ui::ellipsize_text(
            "hello world", 70.f,
            [](const std::string &s) { return measure_w(s, FontWeight::Regular); }));
}

TEST(cut_inside_second_run_keeps_styles) {
  const std::vector<TextSpan> runs{{"AB ", kRed}, {"CDEFGH", kBlue}};
  const auto out =
      afterhours::ui::detail::ellipsize_runs(runs, 70.f, measure_w);
  CHECK(out.size() == 2);
  CHECK(out[0].text == "AB ");
  CHECK(out[0].color.r == 255 && out[0].color.b == 0);
  CHECK(out[1].text == "C...");
  CHECK(out[1].color.r == 0 && out[1].color.b == 255);
  CHECK(drawn_width(out) <= 70.f);
}

TEST(cut_inside_first_run_drops_later_runs) {
  const std::vector<TextSpan> runs{{"abcdefgh", kRed}, {"ij", kBlue}};
  const auto out =
      afterhours::ui::detail::ellipsize_runs(runs, 50.f, measure_w);
  CHECK(out.size() == 1);
  CHECK(out[0].text == "ab...");
  CHECK(out[0].color.r == 255 && out[0].color.b == 0);
  CHECK(drawn_width(out) <= 50.f);
}

TEST(ellipsis_charged_in_cut_run_weight) {
  const auto bold_w = [](const std::string &s, FontWeight w) {
    return static_cast<float>(s.size()) *
           (w == FontWeight::Bold ? 20.f : 10.f);
  };
  const std::vector<TextSpan> runs{{"abcdef", kWhite, FontWeight::Bold}};
  const auto out =
      afterhours::ui::detail::ellipsize_runs(runs, 110.f, bold_w);
  CHECK(joined(out) == "ab...");
  CHECK(out[0].weight == FontWeight::Bold);
}

TEST(tiny_box_yields_ellipsis_only) {
  const std::vector<TextSpan> runs{{"hello", kRed}, {" world", kBlue}};
  const auto out =
      afterhours::ui::detail::ellipsize_runs(runs, 20.f, measure_w);
  CHECK(out.size() == 1);
  CHECK(out[0].text == "...");
  CHECK(out[0].color.r == 255 && out[0].color.b == 0);
}

int main() { return ui_test::run_registered_tests("styled ellipsis"); }
