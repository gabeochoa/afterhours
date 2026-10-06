// styled_text_test.cpp
// Both label renderers draw a styled label from layout_styled_text, and
// AutoLayout sizes one from the same layout.
//
// Recording backend with the harness's stub measurement (10px a byte at 20px,
// baseline at 0.8 of the size), so every expected position is arithmetic.
// Real fonts, fallback glyphs and drawn glyph geometry are text_fallback_test.

#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/styled_text.h>

#include <cstdio>
#include <string>
#include <vector>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

namespace {
constexpr float FS = 20.f;
const Color kRed{255, 0, 0, 255};
const Color kGreen{0, 255, 0, 255};

std::vector<DrawCall> render(ui_test::ImmTestHarness &h, bool batched) {
  if (batched)
    h.render_batched();
  else
    h.render();
  return h.drawn("text");
}

std::string joined(const std::vector<DrawCall> &calls) {
  std::string out;
  for (const auto &c : calls)
    out += c.text;
  return out;
}
} // namespace

// Ellipsis used to be computed for the plain label and then ignored by the
// styled path, which drew every run at full length past its box.
TEST(styled_ellipsis_cuts_the_runs_and_keeps_their_colours) {
  const std::vector<TextSpan> spans{TextSpan{"red part ", kRed},
                                    TextSpan{"green tail that runs long",
                                             kGreen}};
  for (bool batched : {false, true}) {
    ui_test::ImmTestHarness h;
    div(h.context(), mk(h.root(), 0),
        ComponentConfig{}
            .with_size(ComponentSize{pixels(200), pixels(40)})
            .with_absolute_position(0.f, 0.f)
            .with_styled_label(spans)
            .with_text_overflow(TextOverflow::Ellipsis)
            .with_alignment(TextAlignment::Left)
            .with_font_size(FS));
    const auto calls = render(h, batched);
    const Vector2Type inset =
        resolve_text_inset(imm::ThemeDefaults::get().theme);
    const float box_w = 200.f - 2.f * inset.x;
    const StyledLayout expected = layout_styled_text(
        spans,
        StyledFonts{*h.render_font(), UIComponent::DEFAULT_FONT,
                    colors::FontWeight::Regular, FS, 1.f},
        1e9f, box_w);
    CHECK(calls.size() == expected.runs.size());
    if (calls.size() != expected.runs.size() || calls.empty())
      continue;
    const std::string text = joined(calls);
    fprintf(stderr, "        %s: '%s' in %.0f\n",
            batched ? "batched" : "immediate", text.c_str(), box_w);
    CHECK(text.size() > 3 && text.substr(text.size() - 3) == "...");
    CHECK(text.size() * 10.f <= box_w);
    for (size_t i = 0; i < calls.size(); i++) {
      CHECK(calls[i].text == expected.runs[i].text);
      CHECK_APPROX(calls[i].rect.x, inset.x + expected.runs[i].rect.x);
    }
    CHECK(calls.front().color.r == 255 && calls.back().color.g == 255);
  }
}

// A run at twice the size draws at twice the size, sits on the same baseline,
// and pushes the next run along by its own (larger) advance.
TEST(a_scaled_run_shares_the_baseline_and_its_advance) {
  for (bool batched : {false, true}) {
    ui_test::ImmTestHarness h;
    div(h.context(), mk(h.root(), 0),
        ComponentConfig{}
            .with_size(ComponentSize{pixels(400), pixels(80)})
            .with_absolute_position(0.f, 0.f)
            .with_styled_label(
                {TextSpan{"Ab", kRed, colors::FontWeight::Regular, "", 2.f},
                 TextSpan{"cd", kGreen}})
            .with_alignment(TextAlignment::Left)
            .with_font_size(FS));
    const auto calls = render(h, batched);
    CHECK(calls.size() == 2);
    if (calls.size() != 2)
      continue;
    const auto &big = calls[0], &small = calls[1];
    CHECK_APPROX(big.rect.height, 2.f * FS);
    CHECK_APPROX(small.rect.height, FS);
    CHECK_APPROX(big.rect.y + 0.8f * big.rect.height,
                 small.rect.y + 0.8f * small.rect.height);
    CHECK_APPROX(small.rect.x, big.rect.x + 2.f * 2.f * FS * 0.5f);
  }
}

// Dim::Text measures what the renderer will draw: "ab" at double size is 20px
// wider than at the label's size.
TEST(autolayout_sizes_a_styled_label_from_the_layout) {
  ui_test::ImmTestHarness h;
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{Size{Dim::Text}, pixels(60)})
          .with_label("abcd")
          .with_font_size(FS)
          .with_debug_name("plain"));
  div(h.context(), mk(h.root(), 1),
      ComponentConfig{}
          .with_size(ComponentSize{Size{Dim::Text}, pixels(60)})
          .with_styled_label(
              {TextSpan{"ab", kRed, colors::FontWeight::Regular, "", 2.f},
               TextSpan{"cd", kGreen}})
          .with_font_size(FS)
          .with_debug_name("styled"));
  h.layout_only();
  UIComponent *plain = h.find("plain");
  UIComponent *styled = h.find("styled");
  CHECK(plain && styled);
  if (!plain || !styled)
    return;
  fprintf(stderr, "        plain %.1f, styled %.1f\n", plain->rect().width,
          styled->rect().width);
  CHECK_APPROX(styled->rect().width - plain->rect().width, 20.f);
}

int main() { return ui_test::run_registered_tests("styled text"); }
