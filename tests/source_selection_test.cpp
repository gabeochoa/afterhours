// source_selection_test.cpp
// Selection endpoints are source positions, so a selection survives its rows
// scrolling out of a virtual list, and copy returns the source's exact bytes
// -- tabs, CRLF, multi-byte UTF-8 -- or says it is too large.

#include "ui_test_harness.h"
#include <afterhours/src/plugins/ui/source_selection.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using namespace afterhours::ui::source_text;
using ui_test::ImmTestHarness;

namespace {
// Monospace: every codepoint is 10px wide.
float mono(std::string_view s) {
  float w = 0.f;
  for (const char c : s)
    if (!is_continuation(static_cast<unsigned char>(c))) w += 10.f;
  return w;
}

std::string corpus(size_t lines) {
  const char *shapes[] = {"\tint alpha_éλ = %zu;", "日本語\tテキスト %zu", "emoji 🙂\t🙂 %zu",
                          "  spaced\t\ttabs %zu  ", "", "plain ascii line %zu"};
  std::string out;
  char buf[128];
  for (size_t i = 0; i < lines; i++) {
    snprintf(buf, sizeof buf, shapes[i % 6], i);
    out += buf;
    if (i + 1 < lines) out += i % 3 == 0 ? "\r\n" : "\n";
  }
  return out; // no terminator on the last line
}

size_t absolute(const StringSource &s, Position p) { return s.starts[p.line] + p.byte; }

// A virtual list of source lines, and hit-testing against only the rows it
// built this frame.
struct Viewer {
  ImmTestHarness h;
  const StringSource &source;
  EntityID list = -1;
  std::vector<std::pair<size_t, EntityID>> rows;
  static constexpr float row_h = 20.f, view_h = 200.f;

  explicit Viewer(const StringSource &s) : source(s) {}
  void frame() {
    h.begin_frame();
    rows.clear();
    list = virtual_list(h.context(), mk(h.root(), 0), source.line_count(), row_h,
                        [&](size_t i, Entity &row) { rows.emplace_back(i, row.id); },
                        ComponentConfig{}.with_size({pixels(400), pixels(view_h)}))
               .id();
    h.layout_only();
    auto &scroll = this->scroll();
    scroll.viewport_size = {400.f, view_h};
    scroll.content_size = {400.f, row_h * static_cast<float>(source.line_count())};
  }
  HasScrollView &scroll() { return AutoLayout::to_ent_static(list).get<HasScrollView>(); }
  bool rendered(size_t line) const {
    return std::any_of(rows.begin(), rows.end(), [&](auto &r) { return r.first == line; });
  }
  // y relative to the viewport top; clamped into it, as a drag past the
  // edge selects on the edge row.
  Position hit_at(float x, float y) {
    y = std::clamp(y, 0.f, view_h - 1.f);
    const float list_y = AutoLayout::to_cmp_static(list).rect().y;
    for (const auto &[line, id] : rows) {
      const float top = AutoLayout::to_cmp_static(id).rect().y - list_y - scroll().scroll_offset.y;
      if (y >= top && y < top + row_h) {
        const auto d = display_line(content(source.line(line)));
        return hit(line, d, Fragment{0, d.text.size()}, x, mono);
      }
    }
    return {};
  }
};
} // namespace

TEST(selection_beyond_the_viewport_copies_the_exact_source_bytes) {
  const StringSource source(corpus(3000));
  Viewer v(source);
  v.frame();
  Selection sel;
  // Press on line 3 at display column 11, the right half of the first tab in
  // "  spaced\t\ttabs": it snaps to that tab's end.
  sel.press(v.hit_at(10.f * 11 + 2.f, 3 * Viewer::row_h + 5.f), false);
  const Position start = sel.anchor;
  CHECK(start.line == 3);
  CHECK(start.byte == 9);

  // Drag below the view: edge autoscroll carries the list ~2000 lines down.
  for (int frame = 0; frame < 4000 && sel.head.line < 2000; frame++) {
    const float pointer = Viewer::view_h + 40.f;
    const float step = edge_autoscroll(pointer, 0.f, Viewer::view_h, 20.f, 30.f);
    CHECK(step > 0.f);
    v.scroll().jump_to_y(std::min(v.scroll().scroll_offset.y + step, v.scroll().bottom_offset()));
    v.frame();
    sel.drag(v.hit_at(35.f, pointer));
  }
  CHECK(sel.head.line >= 2000);
  CHECK(!v.rendered(start.line)); // the anchor's row is long gone
  CHECK(sel.anchor == start);
  auto copied = copy_text(source, sel.anchor, sel.head, 1 << 20);
  CHECK(copied.status == CopyStatus::Copied);
  CHECK(copied.text == source.text.substr(absolute(source, start),
                                          absolute(source, sel.head) - absolute(source, start)));
  CHECK(copied.text.find("\r\n") != std::string::npos);
  CHECK(copied.text.find("🙂") != std::string::npos);

  // Back to the starting row and before the anchor: a backwards selection.
  v.scroll().jump_to_y(0.f);
  v.frame();
  sel.drag(v.hit_at(0.f, 1 * Viewer::row_h + 5.f));
  CHECK(sel.anchor == start);
  CHECK((sel.head == Position{1, 0}));
  copied = copy_text(source, sel.anchor, sel.head, 1 << 20);
  CHECK(copied.text == source.text.substr(absolute(source, sel.head),
                                          absolute(source, start) - absolute(source, sel.head)));

  // Too large for the caller's limit: refused whole, never truncated.
  const size_t size = absolute(source, {2000, 0}) - absolute(source, start);
  CHECK(copy_text(source, start, {2000, 0}, size - 1).status == CopyStatus::LimitExceeded);
  CHECK(copy_text(source, start, {2000, 0}, size - 1).text.empty());
  CHECK(copy_text(source, start, {2000, 0}, size).text.size() == size);
  CHECK(copy_text(source, start, {5000, 0}, size).status == CopyStatus::OutOfRange);
  CHECK(copy_text(source, start, start, size).status == CopyStatus::Empty);
}

TEST(tabs_map_between_source_and_display) {
  const auto d = display_line("\tab\tc", 4);
  CHECK(d.text == "    ab  c");
  CHECK(to_display(d, 0) == 0);
  CHECK(to_display(d, 1) == 4);
  CHECK(to_display(d, 3) == 6);
  CHECK(to_display(d, 4) == 8);
  CHECK(to_source(d, 1) == 0); // inside the first tab, nearer its start
  CHECK(to_source(d, 3) == 1); // nearer its end
  CHECK(to_source(d, 7) == 4); // the 2-column tab, at its middle
  CHECK(to_source(d, 9) == 5);
  CHECK(hit(0, d, {0, d.text.size()}, 12.f, mono).byte == 0);
  CHECK(hit(0, d, {0, d.text.size()}, 31.f, mono).byte == 1);
  // A multi-byte character is one column and never split.
  const auto u = display_line("é\tx", 4);
  CHECK(u.text == "é   x");
  CHECK(display_column(u, to_display(u, 3)) == 4);
  CHECK(hit(0, u, {0, u.text.size()}, 6.f, mono).byte == 2);
}

TEST(soft_wrapped_fragments_give_each_position_one_row) {
  const auto d = display_line("aaaa bbbb cccccccccc", 4);
  const auto rows = wrap_fragments(d, 60.f, mono);
  CHECK(rows.size() == 4);
  CHECK(rows[0].begin == 0 && rows[0].end == 5);  // "aaaa "
  CHECK(rows[1].begin == 5 && rows[1].end == 10); // "bbbb "
  CHECK(rows[2].end == 16);                       // an over-wide word breaks
  CHECK(fragment_of(rows, 5) == 1);  // a boundary starts the next row
  CHECK(fragment_of(rows, 4) == 0);
  CHECK(fragment_of(rows, d.text.size()) == 3); // the line end stays on the last
  CHECK(hit(0, d, rows[1], 20.f, mono).byte == 7);
  // Highlight geometry for a selection crossing rows.
  const auto mid = row_highlight(0, d, rows[1], {0, 2}, {0, 7}, mono);
  CHECK(mid && mid->first == 0.f && mid->second == 20.f);
  CHECK(!row_highlight(0, d, rows[2], {0, 2}, {0, 7}, mono));
  CHECK(wrap_fragments(display_line(""), 60.f, mono).size() == 1);
}

TEST(keyboard_movement_follows_source_positions) {
  const StringSource source("a🙂b\r\n\tx\nlonger line here\nalpha_éλ beta\n");
  Selection sel;
  move(sel, source, Move::LineEnd, false);
  CHECK((sel.head == Position{0, 6}));
  move(sel, source, Move::Right, false); // across CRLF in one step
  CHECK((sel.head == Position{1, 0}));
  move(sel, source, Move::Left, false); // back across the CRLF
  move(sel, source, Move::Left, false); // over 'b'
  move(sel, source, Move::Left, false); // over the 4-byte emoji in one step
  CHECK((sel.head == Position{0, 1}));
  // Up/Down keep the display column through a tab.
  sel = {};
  sel.anchor = sel.head = {2, 5};
  move(sel, source, Move::Up, false);
  CHECK((sel.head == Position{1, 2})); // "\tx" ends at column 5
  move(sel, source, Move::Down, true);
  CHECK((sel.head == Position{2, 5})); // and the goal column comes back
  CHECK((sel.anchor == Position{1, 2}));
  move(sel, source, Move::Left, false); // collapses to the selection start
  CHECK((sel.head == Position{1, 2}) && sel.empty());
  // Identifier words keep '_' and non-ASCII letters.
  sel.anchor = sel.head = {3, 0};
  move(sel, source, Move::WordRight, true);
  CHECK((sel.head == Position{3, 10}));
  select_word(sel, source, {3, 9});
  CHECK(copy_text(source, sel.anchor, sel.head, 64).text == "alpha_éλ");
  select_line(sel, source, 0);
  CHECK(copy_text(source, sel.anchor, sel.head, 64).text == "a🙂b\r\n");
  move(sel, source, Move::DocumentEnd, false);
  CHECK((sel.head == Position{4, 0}));
  CHECK((clamp(source, {0, 3}) == Position{0, 1})); // mid-codepoint snaps back
}

TEST(edge_autoscroll_ramps_with_distance_past_the_edge) {
  CHECK(edge_autoscroll(100.f, 0.f, 200.f, 20.f, 30.f) == 0.f);
  CHECK(edge_autoscroll(190.f, 0.f, 200.f, 20.f, 30.f) > 0.f);
  CHECK(edge_autoscroll(190.f, 0.f, 200.f, 20.f, 30.f) < edge_autoscroll(230.f, 0.f, 200.f, 20.f, 30.f));
  CHECK(edge_autoscroll(500.f, 0.f, 200.f, 20.f, 30.f) == 30.f);
  CHECK(edge_autoscroll(-500.f, 0.f, 200.f, 20.f, 30.f) == -30.f);
}

int main() { return ui_test::run_registered_tests("source_selection"); }
