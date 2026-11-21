// selectable_text_test.cpp
// Selectable read-only text (decide-selectable-text, option A): a label
// opted in with with_selectable_text() can be pressed, dragged and copied,
// with the one active selection living on UIContext in SOURCE byte offsets.
//
// Two layers, like the rest of the text tests:
//   - the geometry/click helpers, driven with measure = chars * 10 so every
//     offset is arithmetic;
//   - the real systems (ResolveHitTarget + HandleSelectableText) driven
//     frame by frame over an ImmTestHarness, whose measure is
//     chars * font_size * 0.5 -- so a character cell is font_size/2 wide and
//     a line is font_size tall.

#include "ui_test_harness.h"

#include <afterhours/src/plugins/clipboard.h>
#include <afterhours/src/plugins/ui/selectable_text.h>

#include <string>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;
using ui_test::TestInputAction;
namespace ts = afterhours::ui::text_selection;
namespace st = afterhours::ui::selectable_text;
namespace ti = afterhours::text_input;

namespace {
constexpr float CHAR_W = 10.f;
constexpr float LINE_H = 20.f;

float measure(const std::string &s) {
  return static_cast<float>(s.size()) * CHAR_W;
}

// Wrap `text` at `chars` characters' worth of width.
ti::TextLayoutCache layout(const std::string &text, float chars) {
  ti::TextLayoutCache c;
  c.rebuild(text, chars * CHAR_W, LINE_H,
            [](std::string_view s) { return measure(std::string(s)); });
  return c;
}
} // namespace

// ---------------------------------------------------------------------------
// Click streaks, word and line ranges
// ---------------------------------------------------------------------------

TEST(click_streak_counts_within_the_window) {
  ts::ClickStreak streak;
  CHECK(ts::register_click(streak, 1.0f, 5) == 1);
  CHECK(ts::register_click(streak, 1.2f, 6) == 2);
  CHECK(ts::register_click(streak, 1.5f, 5) == 3);
  // Too late, or too far away: a new streak.
  CHECK(ts::register_click(streak, 3.0f, 5) == 1);
  CHECK(ts::register_click(streak, 3.1f, 40) == 1);
}

TEST(word_and_line_ranges) {
  const std::string text = "alpha beta\ngamma delta";
  const ts::Range word = ts::word_range_at(text, 7); // inside "beta"
  CHECK(word.start == 6 && word.end == 10);
  const ts::Range gap = ts::word_range_at(text, 5); // the space
  CHECK(gap.start == 5 && gap.end == 6);
  const ts::Range line = ts::line_range_at(text, 14); // second line
  CHECK(line.start == 11 && line.end == 22);
  CHECK(text.substr(line.start, line.length()) == "gamma delta");
}

TEST(apply_click_takes_word_then_line) {
  const std::string text = "alpha beta gamma";
  ts::Selection sel;
  st::apply_click(sel, text, 7, 1, false);
  CHECK(!sel.has_selection()); // a plain press collapses
  CHECK(sel.cursor == 7);
  st::apply_click(sel, text, 7, 2, false);
  CHECK(sel.range().start == 6 && sel.range().end == 10);
  st::apply_click(sel, text, 7, 3, false);
  CHECK(sel.range().start == 0 && sel.range().end == 16);
  // Shift-click extends from the anchor instead of collapsing.
  sel.collapse_to(2);
  st::apply_click(sel, text, 9, 1, true);
  CHECK(sel.anchor == 2 && sel.cursor == 9);
}

// ---------------------------------------------------------------------------
// Geometry: points and bands over a wrapped block
// ---------------------------------------------------------------------------

TEST(offset_at_and_rects_agree_on_wrapped_text) {
  // 10 cells wide: "alpha beta" fills row 0, "gamma" wraps to row 1.
  const std::string text = "alpha beta gamma";
  auto cache = layout(text, 10.f);
  CHECK(cache.line_count() == 2);

  // Row 1, two characters in: source byte 13 ("alpha beta " is 11, +2).
  const size_t pos = st::offset_at(cache, text,
                                   Vector2Type{2.f * CHAR_W, 1.5f * LINE_H},
                                   LINE_H, TextAlignment::Left, 10.f * CHAR_W,
                                   measure);
  CHECK(pos == 13);

  // Selecting "beta gamma" (6..16) bands the tail of row 0 and all of row 1.
  const auto rects = st::selection_rects(
      cache, text, ts::Range{6, 16}, Vector2Type{100.f, 50.f}, LINE_H,
      TextAlignment::Left, 10.f * CHAR_W, measure);
  CHECK(rects.size() == 2);
  if (rects.size() == 2) {
    CHECK(ui_test::approx(rects[0].x, 100.f + 6.f * CHAR_W));
    CHECK(ui_test::approx(rects[0].width, 4.f * CHAR_W));
    CHECK(ui_test::approx(rects[1].y, 50.f + LINE_H));
    CHECK(ui_test::approx(rects[1].width, 5.f * CHAR_W));
  }
}

TEST(centered_lines_shift_their_bands) {
  const std::string text = "abc";
  auto cache = layout(text, 10.f);
  const auto rects = st::selection_rects(
      cache, text, ts::Range{0, 3}, Vector2Type{0.f, 0.f}, LINE_H,
      TextAlignment::Center, 10.f * CHAR_W, measure);
  CHECK(rects.size() == 1);
  if (!rects.empty())
    CHECK(ui_test::approx(rects[0].x, 3.5f * CHAR_W)); // (10 - 3) / 2 cells

  const size_t pos = st::offset_at(cache, text,
                                   Vector2Type{4.f * CHAR_W, 0.5f * LINE_H},
                                   LINE_H, TextAlignment::Center,
                                   10.f * CHAR_W, measure);
  CHECK(pos == 0); // 4 cells in is still left of the centred text
}

// ---------------------------------------------------------------------------
// The systems: press, drag, copy, clear
// ---------------------------------------------------------------------------

namespace {

// One frame of pointer input through hit resolution and the selectable
// system, exactly as the post-update bridge runs them.
void frame(ImmTestHarness &h, ResolveHitTarget<TestInputAction> &hit,
           HandleSelectableText<TestInputAction> &sel, Entity &label_ent,
           float mx, float my, bool down, bool just_pressed) {
  auto &ctx = h.context();
  ctx.mouse.pos = {mx, my};
  ctx.mouse.left_down = down;
  ctx.mouse.just_pressed = just_pressed;
  hit.once(0.f);
  sel.once(0.016f);
  sel.for_each_with(label_ent, label_ent.get<UIComponent>(),
                    label_ent.get<HasSelectableText>(), 0.f);
  // EndUIContextManager's half of the frame: active survives only while
  // the button is down.
  if (!down)
    ctx.set_active(ctx.ROOT);
}

} // namespace

TEST(selectable_label_selects_drags_copies_and_clears) {
  ImmTestHarness h;
  EntityHelper::registerSingleton<UIContext<TestInputAction>>(
      h.context_entity());
  auto &ctx = h.context();

  auto result = div(ctx, mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size({pixels(400), pixels(60)})
                        .with_absolute_position(0, 0)
                        .with_label("alpha beta gamma delta")
                        .with_selectable_text());
  Entity &ent = result.ent();
  CHECK(ent.has<HasSelectableText>());
  CHECK(ent.has<HasCursor>());
  if (ent.has<HasCursor>())
    CHECK(ent.get<HasCursor>().cursor == CursorType::Text);
  h.layout_only();

  // Harness measure: a character is font_size/2 wide, a line font_size tall.
  auto &cmp = ent.get<UIComponent>();
  const float font_size =
      resolve_to_pixels(cmp.font_size, 600.f, cmp.resolved_scaling_mode,
                        imm::ThemeDefaults::get().theme.ui_scale);
  const float cell = font_size * 0.5f;
  const float mid_y = 30.f; // the single line is vertically centred in 60px

  ResolveHitTarget<TestInputAction> hit;
  HandleSelectableText<TestInputAction> sel_sys;

  // Press at byte 2, drag to byte 9: selects "pha bet".
  frame(h, hit, sel_sys, ent, 2.f * cell + 1.f, mid_y, true, true);
  CHECK(ctx.text_selection_entity == ent.id);
  CHECK(!ctx.has_selection()); // a press alone collapses
  frame(h, hit, sel_sys, ent, 9.f * cell + 1.f, mid_y, true, false);
  CHECK(ctx.has_selection());
  CHECK(ctx.selected_text() == "pha bet");

  // Copy through the real action path, into a memory clipboard.
  clipboard::MemoryProvider memory;
  clipboard::ScopedProvider scoped(memory);
  ctx.last_action = TestInputAction::TextCopy;
  sel_sys.once(0.016f);
  CHECK(memory.get_text() == "pha bet");
  CHECK(ctx.last_action == TestInputAction::None); // consumed, cannot re-fire

  // The band is painted (immediate renderer, recording backend).
  h.render();
  bool band = false;
  for (const auto &call : h.drawn("rectangle"))
    if (call.entity_id == ent.id && call.rect.width > 0.f)
      band = true;
  CHECK(band);

  // A press somewhere else (below the label entirely) clears.
  frame(h, hit, sel_sys, ent, 9.f * cell + 1.f, mid_y, false, false);
  frame(h, hit, sel_sys, ent, 390.f, 300.f, true, true);
  frame(h, hit, sel_sys, ent, 390.f, 300.f, false, false);
  CHECK(!ctx.has_selection());
  CHECK(ctx.text_selection_entity == ctx.ROOT);

  // Two presses inside "beta" (bytes 6..10) within the streak window take
  // the whole word.
  frame(h, hit, sel_sys, ent, 7.f * cell + 1.f, mid_y, true, true);
  frame(h, hit, sel_sys, ent, 7.f * cell + 1.f, mid_y, false, false);
  frame(h, hit, sel_sys, ent, 7.f * cell + 1.f, mid_y, true, true);
  CHECK(ctx.selected_text() == "beta");
}

int main() { return ui_test::run_registered_tests("selectable text"); }
