#pragma once

// Read-only selection over a text source that is only partly rendered.
//
// text_selection.h measures and copies from the lines currently laid out.
// A virtualized log, transcript or code view renders a window of rows, so an
// endpoint whose row scrolled away cannot be resolved from what is rendered.
// Here endpoints are source positions -- line and byte -- that stay valid
// whatever is on screen. The host adapts its text to the TextSource concept
// (line_count(), and line(i) with its terminator); rows supply geometry
// through the display mapping when they are drawn or hit-tested.
//
// Display text expands tabs; a soft-wrapped line is a list of fragments of
// that display text. Copy reads the source a line at a time, keeps tabs, CRLF
// and every other byte as they are, and refuses -- rather than truncates --
// a selection larger than the caller's limit. Revision identity, sides of a
// diff and location headers stay with the host.

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "text_selection.h"

namespace afterhours {
namespace ui {
namespace source_text {

template <typename S>
concept TextSource = requires(const S &s, size_t i) {
  { s.line_count() } -> std::convertible_to<size_t>;
  { s.line(i) } -> std::convertible_to<std::string_view>;
};

/// A line and a byte within its content (terminator excluded).
struct Position {
  size_t line = 0;
  size_t byte = 0;
  auto operator<=>(const Position &) const = default;
};

/// The line without its "\n" or "\r\n".
inline std::string_view content(std::string_view line) {
  if (line.ends_with("\r\n")) return line.substr(0, line.size() - 2);
  if (line.ends_with('\n')) return line.substr(0, line.size() - 1);
  return line;
}

inline bool is_continuation(unsigned char c) { return (c & 0xC0) == 0x80; }

/// A whole string as a source, lines keeping their terminators.
struct StringSource {
  std::string text;
  std::vector<size_t> starts{0};

  explicit StringSource(std::string t) : text(std::move(t)) {
    for (size_t i = 0; i < text.size(); i++)
      if (text[i] == '\n') starts.push_back(i + 1);
  }
  [[nodiscard]] size_t line_count() const { return starts.size(); }
  [[nodiscard]] std::string_view line(size_t i) const {
    const size_t end = i + 1 < starts.size() ? starts[i + 1] : text.size();
    return std::string_view(text).substr(starts[i], end - starts[i]);
  }
};

// ---------------------------------------------------------------------------
// Display mapping
// ---------------------------------------------------------------------------

/// A line as drawn: tabs expanded to the next multiple of tab_width columns.
/// source[d] is the source byte for display byte d; one longer than text.
struct DisplayLine {
  std::string text;
  std::vector<size_t> source;
};

inline DisplayLine display_line(std::string_view line_content, size_t tab_width = 4) {
  DisplayLine out;
  tab_width = std::max<size_t>(tab_width, 1);
  size_t column = 0;
  for (size_t i = 0; i < line_content.size(); i++) {
    const unsigned char c = static_cast<unsigned char>(line_content[i]);
    if (c == '\t') {
      const size_t width = tab_width - column % tab_width;
      out.text.append(width, ' ');
      out.source.insert(out.source.end(), width, i);
      column += width;
      continue;
    }
    out.text += static_cast<char>(c);
    out.source.push_back(i);
    if (!is_continuation(c)) column++;
  }
  out.source.push_back(line_content.size());
  return out;
}

/// The source byte at a display byte. Inside an expanded tab -- a run of
/// display bytes sharing one source byte -- snaps to the nearer edge.
inline size_t to_source(const DisplayLine &d, size_t display_byte) {
  const size_t at = std::min(display_byte, d.source.size() - 1);
  size_t begin = at, end = at;
  while (begin > 0 && d.source[begin - 1] == d.source[at]) begin--;
  while (end + 1 < d.source.size() && d.source[end] == d.source[at]) end++;
  return (at - begin) * 2 < end - begin ? d.source[at] : d.source[end];
}

/// The display byte where a source byte begins.
inline size_t to_display(const DisplayLine &d, size_t source_byte) {
  return static_cast<size_t>(std::lower_bound(d.source.begin(), d.source.end(), source_byte) -
                             d.source.begin());
}

/// Codepoints before display byte d: the column a caret sits in.
inline size_t display_column(const DisplayLine &d, size_t display_byte) {
  size_t column = 0;
  for (size_t i = 0; i < std::min(display_byte, d.text.size()); i++)
    if (!is_continuation(static_cast<unsigned char>(d.text[i]))) column++;
  return column;
}

inline size_t display_byte_at_column(const DisplayLine &d, size_t column) {
  for (size_t i = 0; i < d.text.size(); i++)
    if (!is_continuation(static_cast<unsigned char>(d.text[i])) && column-- == 0) return i;
  return d.text.size();
}

/// One visual row of a soft-wrapped line: display bytes [begin, end).
struct Fragment {
  size_t begin = 0;
  size_t end = 0;
};

/// Greedy soft wrap at `width`, breaking after the last space that fits, or
/// mid-word when a word is wider than the row. Never splits a codepoint.
template <typename MeasureFn>
std::vector<Fragment> wrap_fragments(const DisplayLine &d, float width, MeasureFn &&measure) {
  std::vector<Fragment> out;
  size_t begin = 0;
  while (begin < d.text.size()) {
    size_t end = begin, space = std::string::npos;
    while (end < d.text.size()) {
      size_t next = end + std::max<size_t>(1, utf8_char_length(d.text, end));
      if (width > 0.f && next - begin > 0 && end > begin &&
          measure(std::string_view(d.text).substr(begin, next - begin)) > width)
        break;
      if (d.text[end] == ' ') space = next;
      end = next;
    }
    if (end < d.text.size() && space != std::string::npos && space > begin) end = space;
    out.push_back({begin, end});
    begin = end;
  }
  if (out.empty()) out.push_back({0, 0});
  return out;
}

/// Which fragment a display byte belongs to. A boundary belongs to the
/// fragment it starts -- except the end of the line, which belongs to the
/// last fragment -- so a caret is drawn on exactly one row.
inline size_t fragment_of(const std::vector<Fragment> &rows, size_t display_byte) {
  for (size_t i = 0; i < rows.size(); i++)
    if (display_byte < rows[i].end) return i;
  return rows.empty() ? 0 : rows.size() - 1;
}

/// The source position under x on a rendered row.
template <typename MeasureFn>
Position hit(size_t line, const DisplayLine &d, Fragment row, float x, MeasureFn &&measure) {
  const std::string text = d.text.substr(row.begin, row.end - row.begin);
  const size_t within = text_selection::detail_sel::offset_nearest_x(
      text, x, [&](const std::string &s) { return measure(std::string_view(s)); });
  return {line, to_source(d, row.begin + within)};
}

/// The highlight's horizontal extent on a rendered row, if the selection
/// [first, last) touches it.
template <typename MeasureFn>
std::optional<std::pair<float, float>> row_highlight(size_t line, const DisplayLine &d, Fragment row,
                                                     Position first, Position last,
                                                     MeasureFn &&measure) {
  if (line < first.line || line > last.line || first == last) return std::nullopt;
  const size_t from = line == first.line ? std::max(row.begin, to_display(d, first.byte)) : row.begin;
  const size_t to = line == last.line ? std::min(row.end, to_display(d, last.byte)) : row.end;
  if (to < from || (to == from && line == last.line)) return std::nullopt;
  const std::string_view text(d.text);
  return std::pair{measure(text.substr(row.begin, from - row.begin)),
                   measure(text.substr(row.begin, to - row.begin))};
}

// ---------------------------------------------------------------------------
// Selection and movement
// ---------------------------------------------------------------------------

/// Identifier words by default: ASCII letters, digits, '_' and any non-ASCII
/// byte, so `alpha_éλ` is one word.
inline bool identifier_byte(unsigned char c) {
  return c >= 0x80 || c == '_' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z');
}
using WordPolicy = bool (*)(unsigned char);

enum struct Move {
  Left, Right, WordLeft, WordRight, Up, Down, LineStart, LineEnd,
  PageUp, PageDown, DocumentStart, DocumentEnd,
};

struct Selection {
  Position anchor;
  Position head;
  // Display column Up/Down aim for, so a short line does not lose it.
  std::optional<size_t> goal_column;

  [[nodiscard]] bool empty() const { return anchor == head; }
  [[nodiscard]] std::pair<Position, Position> ordered() const {
    return anchor <= head ? std::pair{anchor, head} : std::pair{head, anchor};
  }
  /// Mouse down (shift-click extends), then drag.
  void press(Position p, bool extend) {
    head = p;
    if (!extend) anchor = p;
    goal_column.reset();
  }
  void drag(Position p) {
    head = p;
    goal_column.reset();
  }
};

/// A position moved onto the source: an existing line, inside its content,
/// on a codepoint boundary.
template <TextSource S> Position clamp(const S &source, Position p) {
  if (source.line_count() == 0) return {};
  p.line = std::min(p.line, source.line_count() - 1);
  const auto line = source.line(p.line);
  const std::string_view text = content(line);
  p.byte = std::min(p.byte, text.size());
  while (p.byte > 0 && p.byte < text.size() && is_continuation(static_cast<unsigned char>(text[p.byte])))
    p.byte--;
  return p;
}

template <TextSource S>
void move(Selection &sel, const S &source, Move how, bool extend, size_t page_lines = 20,
          size_t tab_width = 4, WordPolicy word = identifier_byte) {
  const size_t lines = source.line_count();
  if (lines == 0) return;
  // Collapsing a selection with Left/Right lands on its edge.
  if (!extend && !sel.empty() && (how == Move::Left || how == Move::Right)) {
    const auto [first, last] = sel.ordered();
    sel.anchor = sel.head = how == Move::Left ? first : last;
    sel.goal_column.reset();
    return;
  }
  Position p = clamp(source, sel.head);
  const auto line_text = [&](size_t i) {
    const auto line = source.line(i);
    return std::string(content(line));
  };
  std::string text = line_text(p.line);
  const auto vertical = [&](long delta) {
    const DisplayLine here = display_line(text, tab_width);
    if (!sel.goal_column) sel.goal_column = display_column(here, to_display(here, p.byte));
    const long target = std::clamp(static_cast<long>(p.line) + delta, 0L, static_cast<long>(lines) - 1);
    if (target == static_cast<long>(p.line)) {
      p.byte = delta < 0 ? 0 : text.size();
      return;
    }
    p.line = static_cast<size_t>(target);
    const DisplayLine there = display_line(line_text(p.line), tab_width);
    p.byte = to_source(there, display_byte_at_column(there, *sel.goal_column));
  };
  const bool keeps_goal = how == Move::Up || how == Move::Down || how == Move::PageUp ||
                          how == Move::PageDown;
  switch (how) {
  case Move::Left:
    if (p.byte > 0) {
      do p.byte--;
      while (p.byte > 0 && is_continuation(static_cast<unsigned char>(text[p.byte])));
    } else if (p.line > 0) {
      p.line--;
      p.byte = line_text(p.line).size();
    }
    break;
  case Move::Right:
    if (p.byte < text.size()) p.byte += std::max<size_t>(1, utf8_char_length(text, p.byte));
    else if (p.line + 1 < lines) p = {p.line + 1, 0};
    break;
  case Move::WordLeft:
    if (p.byte == 0 && p.line > 0) { p.line--; text = line_text(p.line); p.byte = text.size(); }
    while (p.byte > 0 && !word(static_cast<unsigned char>(text[p.byte - 1]))) p.byte--;
    while (p.byte > 0 && word(static_cast<unsigned char>(text[p.byte - 1]))) p.byte--;
    break;
  case Move::WordRight:
    if (p.byte == text.size() && p.line + 1 < lines) { p = {p.line + 1, 0}; text = line_text(p.line); }
    while (p.byte < text.size() && !word(static_cast<unsigned char>(text[p.byte]))) p.byte++;
    while (p.byte < text.size() && word(static_cast<unsigned char>(text[p.byte]))) p.byte++;
    break;
  case Move::Up: vertical(-1); break;
  case Move::Down: vertical(1); break;
  case Move::PageUp: vertical(-static_cast<long>(page_lines)); break;
  case Move::PageDown: vertical(static_cast<long>(page_lines)); break;
  case Move::LineStart: p.byte = 0; break;
  case Move::LineEnd: p.byte = text.size(); break;
  case Move::DocumentStart: p = {}; break;
  case Move::DocumentEnd: p = {lines - 1, line_text(lines - 1).size()}; break;
  }
  if (!keeps_goal) sel.goal_column.reset();
  sel.head = p;
  if (!extend) sel.anchor = p;
}

/// Double-click: the word (or run of non-word bytes) at p.
template <TextSource S>
void select_word(Selection &sel, const S &source, Position p, WordPolicy word = identifier_byte) {
  p = clamp(source, p);
  const auto line = source.line(p.line);
  const std::string_view text = content(line);
  if (text.empty()) { sel.anchor = sel.head = p; return; }
  const size_t at = std::min(p.byte, text.size() - 1);
  const bool kind = word(static_cast<unsigned char>(text[at]));
  size_t begin = at, end = at;
  while (begin > 0 && word(static_cast<unsigned char>(text[begin - 1])) == kind) begin--;
  while (end < text.size() && word(static_cast<unsigned char>(text[end])) == kind) end++;
  while (end < text.size() && is_continuation(static_cast<unsigned char>(text[end]))) end++;
  sel.anchor = {p.line, begin};
  sel.head = {p.line, end};
  sel.goal_column.reset();
}

/// Triple-click: the whole line, through its terminator.
template <TextSource S> void select_line(Selection &sel, const S &source, size_t line) {
  const Position p = clamp(source, {line, 0});
  sel.anchor = p;
  sel.head = p.line + 1 < source.line_count() ? Position{p.line + 1, 0}
                                               : Position{p.line, content(source.line(p.line)).size()};
  sel.goal_column.reset();
}

/// Pixels to scroll this frame while dragging near or past an edge of the
/// view along one axis: negative toward the start, zero inside the margins,
/// up to max_step once the pointer is a margin's width beyond the edge.
inline float edge_autoscroll(float pointer, float view_start, float view_end, float margin,
                             float max_step) {
  margin = std::max(margin, 1.f);
  if (pointer < view_start + margin)
    return -max_step * std::min(1.f, (view_start + margin - pointer) / (2.f * margin));
  if (pointer > view_end - margin)
    return max_step * std::min(1.f, (pointer - view_end + margin) / (2.f * margin));
  return 0.f;
}

// ---------------------------------------------------------------------------
// Copy
// ---------------------------------------------------------------------------

enum struct CopyStatus {
  Copied,
  Empty,         // nothing selected
  OutOfRange,    // an endpoint is not in this source (it changed underneath)
  LimitExceeded, // larger than the caller's limit; nothing copied
};

struct CopyResult {
  CopyStatus status = CopyStatus::Empty;
  std::string text;
};

/// The exact source bytes between two positions, terminators included where
/// the range crosses them. Reads one line at a time and stops as soon as the
/// output would pass `limit` bytes.
template <TextSource S>
CopyResult copy_text(const S &source, Position a, Position b, size_t limit) {
  if (a > b) std::swap(a, b);
  const auto valid = [&](Position p) {
    if (p.line >= source.line_count()) return false;
    const auto line = source.line(p.line);
    const std::string_view text = content(line);
    return p.byte <= text.size() &&
           (p.byte == text.size() || !is_continuation(static_cast<unsigned char>(text[p.byte])));
  };
  if (!valid(a) || !valid(b)) return {CopyStatus::OutOfRange, {}};
  if (a == b) return {CopyStatus::Empty, {}};
  CopyResult out{CopyStatus::Copied, {}};
  for (size_t i = a.line; i <= b.line; i++) {
    const auto line = source.line(i);
    const std::string_view whole = line;
    const size_t begin = i == a.line ? a.byte : 0;
    const size_t end = i == b.line ? b.byte : whole.size();
    if (end - begin > limit - out.text.size()) return {CopyStatus::LimitExceeded, {}};
    out.text.append(whole.substr(begin, end - begin));
  }
  return out;
}

} // namespace source_text
} // namespace ui
} // namespace afterhours
