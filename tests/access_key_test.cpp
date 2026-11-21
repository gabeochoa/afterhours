// R-097: Alt-style access keys. "&File" displays "File" with the F
// underlined; Alt+F activates the row. "&&" is an escaped literal '&', a
// trailing '&' is literal, and plain labels pass through untouched.

#include "ui_test_harness.h"

#include <afterhours/src/plugins/e2e_testing/input_injector.h>
#include <afterhours/src/plugins/ui/access_key.h>
#include <afterhours/src/plugins/ui/menu.h>

#include <cmath>
#include <string>
#include <vector>

using namespace afterhours;
using namespace afterhours::ui;
using ui_test::ImmTestHarness;
using namespace afterhours::ui::imm;

namespace {

std::vector<MenuItem> two_rows() {
  return {MenuItem{"&File", "", false, false},
          MenuItem{"Safe", "", false, false}};
}

}

TEST(access_key_parse_marked_label) {
  const auto p = parse_access_key("&File");
  ui_test::check(p.has_access && p.display == "File" && p.underline_index == 0 &&
                    p.key == 'F',
                "&File -> File, underlined F", __FILE__, __LINE__);
  const auto q = parse_access_key("Saf&e");
  ui_test::check(q.display == "Safe" && q.underline_index == 3 && q.key == 'e',
                "Saf&e -> Safe, underlined e", __FILE__, __LINE__);
}

TEST(access_key_parse_escapes_and_plain) {
  const auto esc = parse_access_key("R&&D");
  ui_test::check(!esc.has_access && esc.display == "R&D",
                "&& is a literal ampersand, not a marker", __FILE__,
                __LINE__);
  const auto trail = parse_access_key("Save&");
  ui_test::check(!trail.has_access && trail.display == "Save&",
                "a trailing & has no char to mark and stays literal",
                __FILE__, __LINE__);
  const auto plain = parse_access_key("Plain");
  ui_test::check(!plain.has_access && plain.display == "Plain" &&
                    plain.underline_index == -1,
                "a label with no marker is unchanged", __FILE__, __LINE__);
  const auto later = parse_access_key("A&B&C");
  ui_test::check(later.display == "AB&C" && later.has_access &&
                    later.underline_index == 1,
                "only the first marker counts", __FILE__, __LINE__);
}

TEST(access_key_char_to_code) {
  ui_test::check(key_code_for_access_char('f') == keys::F &&
                    key_code_for_access_char('F') == keys::F,
                "letter case is ignored", __FILE__, __LINE__);
  ui_test::check(key_code_for_access_char('5') == keys::FIVE,
                "digit access chars work", __FILE__, __LINE__);
  ui_test::check(key_code_for_access_char('!') == 0,
                "non-key chars cannot be pressed", __FILE__, __LINE__);
}

TEST(access_key_underline_rect_covers_the_glyph) {
  const auto r = afterhours::ui::access_underline_rect(10.f, 20.f, 16.f, 8.f, 8.f);
  ui_test::check(ui_test::approx(r.x, 18.f) &&
                    ui_test::approx(r.y, 35.f) && ui_test::approx(r.width, 8.f) &&
                    ui_test::approx(r.height, 1.f),
                "1px line over the glyph at the line-box bottom", __FILE__,
                __LINE__);
}

TEST(menu_row_strips_marker_and_seeds_underline) {
  ImmTestHarness h;
  bool open = true;
  h.begin_frame();
  context_menu(h.context(), mk(h.root(), 0), two_rows(), Vector2Type{100, 100},
               open, ComponentConfig{}.with_size(
                         ComponentSize{pixels(180), pixels(28)}));
  h.layout_only();
  auto row = UICollectionHolder::getEntityForID(h.find("menu_item_0")->id);
  const auto &lbl = row->get<HasLabel>();
  ui_test::check(lbl.label == "File" && lbl.underline_index == 0,
                "the marker is stripped and the index seeded", __FILE__,
                __LINE__);
  auto plain = UICollectionHolder::getEntityForID(h.find("menu_item_1")->id);
  ui_test::check(plain->get<HasLabel>().label == "Safe" &&
                    plain->get<HasLabel>().underline_index == -1,
                "a plain row gains no underline", __FILE__, __LINE__);
}

TEST(menu_alt_char_activates_the_row) {
  ImmTestHarness h;
  bool open = true;
  testing::input_injector::reset_all();
  testing::input_injector::set_key_held(keys::LEFT_ALT);
  testing::input_injector::set_key_down(keys::F);
  testing::input_injector::reset_frame();
  h.begin_frame();
  const int chosen =
      context_menu(h.context(), mk(h.root(), 0), two_rows(),
                   Vector2Type{100, 100}, open,
                   ComponentConfig{}.with_size(
                       ComponentSize{pixels(180), pixels(28)}));
  ui_test::check(chosen == 0, "Alt+F chooses the &File row", __FILE__,
                 __LINE__);
  testing::input_injector::reset_all();
}

TEST(menu_char_without_alt_activates_nothing) {
  ImmTestHarness h;
  bool open = true;
  testing::input_injector::set_key_down(keys::F);
  testing::input_injector::reset_frame();
  h.begin_frame();
  const int chosen =
      context_menu(h.context(), mk(h.root(), 0), two_rows(),
                   Vector2Type{100, 100}, open,
                   ComponentConfig{}.with_size(
                       ComponentSize{pixels(180), pixels(28)}));
  ui_test::check(chosen == kNoMenuSelection, "a bare F press does nothing",
                 __FILE__, __LINE__);
  testing::input_injector::reset_all();
}

TEST(underline_is_drawn_under_the_marked_glyph) {
  ImmTestHarness h;
  h.begin_frame();
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}.with_access_label("Saf&e").with_size(
          ComponentSize{pixels(120), pixels(24)}));
  const auto &calls = h.render();

  float text_x = -1.f, text_h = 0.f;
  for (const auto &c : h.drawn("text"))
    if (c.text == "Safe") {
      text_x = c.rect.x;
      text_h = c.rect.height;
    }
  ui_test::check(text_x >= 0.f, "the marked label renders", __FILE__,
                 __LINE__);
  bool found = false;
  for (const auto &c : calls) {
    if (c.op == "text" || c.rect.height != 1.f)
      continue;
    if (c.rect.x >= text_x && c.rect.x < text_x + 4.f * text_h * 0.5f)
      found = ui_test::approx(c.rect.x - text_x, 3.f * text_h * 0.5f) &&
              ui_test::approx(c.rect.width, text_h * 0.5f);
  }
  ui_test::check(found, "a 1px rect covers the e glyph", __FILE__, __LINE__);
}

TEST(plain_label_draws_no_underline) {
  ImmTestHarness h;
  h.begin_frame();
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}.with_label("Plain").with_size(
          ComponentSize{pixels(120), pixels(24)}));
  const auto &calls = h.render();
  bool found = false;
  for (const auto &c : calls)
    if (c.op != "text" && c.rect.height == 1.f)
      found = true;
  ui_test::check(!found, "no underline rect for a plain label", __FILE__,
                 __LINE__);
}

int main() { return ui_test::run_registered_tests("access_key"); }
