#define AFTERHOURS_DEBUG_TEXT_OVERFLOW 1
#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

static ComponentConfig positioned(float x, float y, float w, float h) {
  return ComponentConfig{}.with_size({pixels(w), pixels(h)})
      .with_absolute_position(x, y).with_corner_radius(0)
      .with_skip_grid_snap(true).with_transparent_bg();
}

struct Drawn {
  std::vector<DrawCall> texts;
  std::vector<DrawCall> red;
};

static Drawn run(bool batched, TextOverflow mode, float w, float h, float fs,
                 const std::string &label) {
  ImmTestHarness harness;
  div(harness.context(), mk(harness.root(), 0),
      positioned(10, 10, w, h).with_label(label)
          .with_font(UIComponent::DEFAULT_FONT, pixels(fs))
          .with_text_overflow(mode));
  if (batched) harness.render_batched(); else harness.render();
  Drawn out;
  out.texts = harness.drawn("text");
  for (const char *op : {"rectangle", "rectangle_rounded"})
    for (auto &c : harness.drawn(op))
      if (c.color.r == 255 && c.color.g == 50 && c.color.b == 50)
        out.red.push_back(c);
  return out;
}

static void check_same_red(const Drawn &a, const Drawn &b) {
  CHECK(a.red.size() == b.red.size());
  for (size_t i = 0; i < a.red.size() && i < b.red.size(); i++) {
    CHECK_APPROX(a.red[i].rect.x, b.red[i].rect.x);
    CHECK_APPROX(a.red[i].rect.y, b.red[i].rect.y);
    CHECK_APPROX(a.red[i].rect.width, b.red[i].rect.width);
    CHECK_APPROX(a.red[i].rect.height, b.red[i].rect.height);
    CHECK(a.red[i].color.a == b.red[i].color.a);
  }
}

static void check_parity(TextOverflow mode, float w, float h, float fs,
                         const std::string &label, bool expect_debug) {
  Drawn imm = run(false, mode, w, h, fs, label);
  Drawn bat = run(true, mode, w, h, fs, label);
  CHECK(imm.texts.size() == 1);
  CHECK(bat.texts.size() == 1);
  if (!imm.texts.empty() && !bat.texts.empty())
    CHECK(imm.texts[0].text == bat.texts[0].text);
  if (expect_debug) {
    CHECK(imm.red.size() == 5); // overlay + 4 border sides
    check_same_red(imm, bat);
  } else {
    CHECK(imm.red.empty());
    CHECK(bat.red.empty());
  }
}

TEST(clip_overflow_parity_150x40) {
  const std::string label = "The quick brown fox jumps over the lazy dog";
  check_parity(TextOverflow::Clip, 150, 40, 16, label, true);
  Drawn imm = run(false, TextOverflow::Clip, 150, 40, 16, label);
  CHECK(!imm.texts.empty() && imm.texts[0].text == label);
}

TEST(clip_overflow_parity_90x34) {
  check_parity(TextOverflow::Clip, 90, 34, 14,
               "Long text in narrow space gets truncated nicely", true);
}

TEST(ellipsis_overflow_parity_150x40) {
  const std::string label = "The quick brown fox jumps over the lazy dog";
  check_parity(TextOverflow::Ellipsis, 150, 40, 16, label, false);
  Drawn imm = run(false, TextOverflow::Ellipsis, 150, 40, 16, label);
  CHECK(!imm.texts.empty() && imm.texts[0].text.size() < label.size());
  CHECK(!imm.texts.empty() &&
        imm.texts[0].text.compare(imm.texts[0].text.size() - 3, 3, "...") == 0);
}

TEST(ellipsis_overflow_parity_90x34) {
  const std::string label = "Long text in narrow space gets truncated nicely";
  check_parity(TextOverflow::Ellipsis, 90, 34, 14, label, false);
  Drawn imm = run(false, TextOverflow::Ellipsis, 90, 34, 14, label);
  CHECK(!imm.texts.empty() && imm.texts[0].text.size() < label.size());
  CHECK(!imm.texts.empty() &&
        imm.texts[0].text.compare(imm.texts[0].text.size() - 3, 3, "...") == 0);
}

TEST(ellipsis_width_is_not_overflow_at_explicit_size) {
  ImmTestHarness h;
  auto &fm = *h.render_font();
  RectangleType box{0, 0, 150, 40};
  auto clip = position_text_ex(fm, "The quick brown fox jumps over", box,
                               TextAlignment::Left, Vector2Type{0, 0}, 16.f,
                               0.f, TextOverflow::Clip, false);
  auto ell = position_text_ex(fm, "The quick brown fox jumps over", box,
                              TextAlignment::Left, Vector2Type{0, 0}, 16.f,
                              0.f, TextOverflow::Ellipsis, false);
  CHECK(!clip.text_fits);
  CHECK(ell.text_fits);
}

int main() { return ui_test::run_registered_tests("batched text overflow"); }
